/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 */

#pragma once

#include "VulkanRTCommon.h"
#include "VulkanUtils.h"
#include "SimpleUtils.h"
#include "Vulkan3DGRTModel.h"

#include "AABB_Clipping.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <vector>
//#include <unordered_map>

using namespace std;

class SplitBLAS {
private:
	uint32_t gaussianCnt;
	vks::VulkanDevice* vulkanDevice;

	glm::vec3 minPos{ FLT_MAX ,FLT_MAX ,FLT_MAX }, maxPos{ -FLT_MAX,-FLT_MAX,-FLT_MAX };

	vector<float> vertices;
	vector<uint32_t> indices;
	uint32_t vertCnt;
	uint32_t idxCnt;
	
	uint32_t numCellsTotal;
	vector<vector<glm::vec3>> h_splittedVert;
	vector<vector<float>> h_splittedVertFP;
	vector<vector<uint32_t>> h_splittedIdx;
	vector<vector<uint32_t>> h_splittedPrimitiveId;
	vector<uint64_t> h_splittedPrimitiveIdsDeviceAddress;

	struct Attribute {
		uint32_t count;
		vks::Buffer buffer;
	};

	/* createBLAS */
	PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
	PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
	PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
	PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
	PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
	PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
	PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;

	struct AccelerationStructure {
		VkAccelerationStructureKHR handle;
		uint64_t deviceAddress = 0;
		VkDeviceMemory memory;
		VkBuffer buffer;

		void destroy(VkDevice device) {
			if (buffer)
			{
				vkDestroyBuffer(device, buffer, nullptr);
			}
			if (memory)
			{
				vkFreeMemory(device, memory, nullptr);
			}
		}
	};

	struct ScratchBuffer
	{
		uint64_t deviceAddress = 0;
		VkBuffer handle = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};

	VkTransformMatrixKHR tMat{};
	vks::Buffer tMatBuffer;

	void copyDeviceToHost(vks::Buffer& vertexBuffer, vks::Buffer& indexBuffer, VkQueue& queue) {
		vertices.resize(vertexBuffer.size);
		indices.resize(indexBuffer.size);
		vertCnt = vertexBuffer.size;
		idxCnt = indexBuffer.size;

		/*** Vetices ***/
		vks::Buffer stagingBuffer;
		stagingBuffer.size = vertexBuffer.size;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.size, &stagingBuffer.buffer, &stagingBuffer.memory, nullptr));

		VkBufferCopy copyRegion;
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = stagingBuffer.size;
		vulkanDevice->copyBuffer(&vertexBuffer, &stagingBuffer, queue, &copyRegion);

		void* data;
		vkMapMemory(vulkanDevice->logicalDevice, stagingBuffer.memory, 0, stagingBuffer.size, 0, &data);
		memcpy(vertices.data(), data, stagingBuffer.size);
		vkUnmapMemory(vulkanDevice->logicalDevice, stagingBuffer.memory);

		vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer.buffer, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, stagingBuffer.memory, nullptr);

		/*** Indices ***/
		stagingBuffer.size = indexBuffer.size;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.size, &stagingBuffer.buffer, &stagingBuffer.memory, nullptr));

		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = stagingBuffer.size;
		vulkanDevice->copyBuffer(&indexBuffer, &stagingBuffer, queue, &copyRegion);

		void* data2;
		vkMapMemory(vulkanDevice->logicalDevice, stagingBuffer.memory, 0, stagingBuffer.size, 0, &data2);
		memcpy(indices.data(), data2, stagingBuffer.size);
		vkUnmapMemory(vulkanDevice->logicalDevice, stagingBuffer.memory);

		vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer.buffer, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, stagingBuffer.memory, nullptr);
	}

	void saveGeometries_SBLAS(std::vector<glm::vec3>& vertexBuffer, std::vector<uint32_t>& indexBuffer) {
		/* split cells with aabb-triangle clipping */
		for (int i = 0; i < vertexBuffer.size(); i++) {
			if (vertexBuffer[i].x > maxPos.x) maxPos.x = vertexBuffer[i].x;
			if (vertexBuffer[i].x < minPos.x) minPos.x = vertexBuffer[i].x;
			if (vertexBuffer[i].y > maxPos.y) maxPos.y = vertexBuffer[i].y;
			if (vertexBuffer[i].y < minPos.y) minPos.y = vertexBuffer[i].y;
			if (vertexBuffer[i].z > maxPos.z) maxPos.z = vertexBuffer[i].z;
			if (vertexBuffer[i].z < minPos.z) minPos.z = vertexBuffer[i].z;
		}

		maxPos += SCENE_EPSILON;
		minPos -= SCENE_EPSILON;

		const glm::vec3 sceneSize = maxPos - minPos;
		const float maxSize = glm::max(glm::max(sceneSize.x, sceneSize.y), sceneSize.z);
		const float gridSize = maxSize / NUMBER_OF_CELLS_PER_LONGEST_AXIS;
		const glm::ivec3 numCells = glm::ivec3(
			(maxSize == sceneSize.x) ? NUMBER_OF_CELLS_PER_LONGEST_AXIS : static_cast<int>(sceneSize.x / gridSize) + 1,
			(maxSize == sceneSize.y) ? NUMBER_OF_CELLS_PER_LONGEST_AXIS : static_cast<int>(sceneSize.y / gridSize) + 1,
			(maxSize == sceneSize.z) ? NUMBER_OF_CELLS_PER_LONGEST_AXIS : static_cast<int>(sceneSize.z / gridSize) + 1
		);
		numCellsTotal = numCells.x * numCells.y * numCells.z;

		h_splittedVert.resize(numCellsTotal);
		h_splittedIdx.resize(numCellsTotal);
		h_splittedPrimitiveId.resize(numCellsTotal);

		auto calcIdx = [numCells](glm::ivec3 idx)->int {return numCells.x * numCells.y * idx.z + numCells.x * idx.y + idx.x; };
		assert(indexBuffer.size() % 3 == 0);

		std::vector<AABB_Triangle_Clipping::_AABB> gridAabb(numCellsTotal);

		//split space by square cells
		for (int i = 0; i < numCells.x; ++i) {
			for (int j = 0; j < numCells.y; ++j) {
				for (int k = 0; k < numCells.z; ++k) {
					gridAabb[calcIdx({ i, j, k })] = {
						minPos.x + i * gridSize, minPos.y + j * gridSize, minPos.z + k * gridSize,
						minPos.x + (i + 1) * gridSize, minPos.y + (j + 1) * gridSize, minPos.z + (k + 1) * gridSize
					};
				}
			}
		}

		std::vector<std::unordered_map<glm::vec3, uint32_t>> uniqueVertices(numCellsTotal);

		// split vertexes
		uint32_t primitiveId = 0;
		for (int i = 0; i < indexBuffer.size(); i += 3) {
			glm::vec3 tri[3] = { vertexBuffer[indexBuffer[i]], vertexBuffer[indexBuffer[i + 1]], vertexBuffer[indexBuffer[i + 2]] };
			glm::vec3 triMinPos = glm::min(glm::min(tri[0], tri[1]), tri[2]);
			glm::vec3 triMaxPos = glm::max(glm::max(tri[0], tri[1]), tri[2]);

			for (int cellIdx = 0; cellIdx < numCellsTotal; ++cellIdx) {
				if (gridAabb[cellIdx].xmax < triMinPos.x || gridAabb[cellIdx].xmin > triMaxPos.x) continue;
				if (gridAabb[cellIdx].ymax < triMinPos.y || gridAabb[cellIdx].ymin > triMaxPos.y) continue;
				if (gridAabb[cellIdx].zmax < triMinPos.z || gridAabb[cellIdx].zmin > triMaxPos.z) continue;

				//TODO : test aabb-triangle intersection before clipping
				std::vector<glm::vec3> polygon;
				AABB_Triangle_Clipping::_clip_triangle_against_AABB(tri, gridAabb[cellIdx], polygon);

				if (polygon.size() == 0) continue;
				for (int k = 1; k < polygon.size() - 1; ++k) {
					if (uniqueVertices[cellIdx].count(polygon[0]) == 0) {
						uniqueVertices[cellIdx][polygon[0]] = static_cast<uint32_t>(h_splittedVert[cellIdx].size());
						h_splittedVert[cellIdx].push_back(polygon[0]);
					}
					h_splittedIdx[cellIdx].push_back(uniqueVertices[cellIdx][polygon[0]]);

					if (uniqueVertices[cellIdx].count(polygon[k]) == 0) {
						uniqueVertices[cellIdx][polygon[k]] = static_cast<uint32_t>(h_splittedVert[cellIdx].size());
						h_splittedVert[cellIdx].push_back(polygon[k]);
					}
					h_splittedIdx[cellIdx].push_back(uniqueVertices[cellIdx][polygon[k]]);

					if (uniqueVertices[cellIdx].count(polygon[k + 1]) == 0) {
						uniqueVertices[cellIdx][polygon[k + 1]] = static_cast<uint32_t>(h_splittedVert[cellIdx].size());
						h_splittedVert[cellIdx].push_back(polygon[k + 1]);
					}
					h_splittedIdx[cellIdx].push_back(uniqueVertices[cellIdx][polygon[k + 1]]);

					h_splittedPrimitiveId[cellIdx].push_back(primitiveId / 20);
				}
			}
			primitiveId++;
		}
	}

	void copyToDevice(VkQueue& queue) {
		struct StagingBuffer {
			VkBuffer buffer;
			VkDeviceMemory memory;
		};

		VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		int totalVert = 0;
		int totalVertSize = 0;
		int totalIndex = 0;
		int totalIndexSize = 0;
		int totalPrimitiveId = 0;
		int totalPrimitiveIdSize = 0;
		for (int i = 0; i < numCellsTotal; i++) {
			size_t vertexBufferSize = h_splittedVertFP[i].size() * sizeof(float);
			size_t indexBufferSize = h_splittedIdx[i].size() * sizeof(uint32_t);
			size_t primitiveIdBufferSize = h_splittedPrimitiveId[i].size() * sizeof(uint32_t);
			if (vertexBufferSize == 0) continue;

			Attribute cellVertices;
			Attribute cellIndices;
			Attribute cellPrimitiveIds;
			cellVertices.count = static_cast<uint32_t>(h_splittedVert[i].size());
			cellIndices.count = static_cast<uint32_t>(h_splittedIdx[i].size());
			cellPrimitiveIds.count = static_cast<uint32_t>(h_splittedPrimitiveId[i].size());
			totalVert += cellVertices.count;
			totalVertSize += vertexBufferSize;
			totalIndex += cellIndices.count;
			totalIndexSize += indexBufferSize;
			totalPrimitiveId += cellPrimitiveIds.count;
			totalPrimitiveIdSize += primitiveIdBufferSize;

			StagingBuffer vertexStaging, indexStaging, primitiveIdStaging;

			// Vertex
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				vertexBufferSize,
				&vertexStaging.buffer,
				&vertexStaging.memory,
				h_splittedVertFP[i].data()));
			// Index
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				indexBufferSize,
				&indexStaging.buffer,
				&indexStaging.memory,
				h_splittedIdx[i].data()));
			// Primitive
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				primitiveIdBufferSize,
				&primitiveIdStaging.buffer,
				&primitiveIdStaging.memory,
				h_splittedPrimitiveId[i].data()));

			// Create device local buffers
			// Vertex buffer
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsageFlags,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				vertexBufferSize,
				&cellVertices.buffer.buffer,
				&cellVertices.buffer.memory));
			// Index buffer
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsageFlags,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				indexBufferSize,
				&cellIndices.buffer.buffer,
				&cellIndices.buffer.memory));
			// Primitive Id buffer
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsageFlags,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				primitiveIdBufferSize,
				&cellPrimitiveIds.buffer.buffer,
				&cellPrimitiveIds.buffer.memory));

			// Copy from staging buffers
			VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			VkBufferCopy copyRegion = {};

			copyRegion.size = vertexBufferSize;
			vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, cellVertices.buffer.buffer, 1, &copyRegion);

			copyRegion.size = indexBufferSize;
			vkCmdCopyBuffer(copyCmd, indexStaging.buffer, cellIndices.buffer.buffer, 1, &copyRegion);

			copyRegion.size = primitiveIdBufferSize;
			vkCmdCopyBuffer(copyCmd, primitiveIdStaging.buffer, cellPrimitiveIds.buffer.buffer, 1, &copyRegion);

			d_splittedVertices.push_back(cellVertices);
			d_splittedIndices.push_back(cellIndices);
			d_splittedPrimitiveIds.push_back(cellPrimitiveIds);

			vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

			vkDestroyBuffer(vulkanDevice->logicalDevice, vertexStaging.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, vertexStaging.memory, nullptr);
			vkDestroyBuffer(vulkanDevice->logicalDevice, indexStaging.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, indexStaging.memory, nullptr);
			vkDestroyBuffer(vulkanDevice->logicalDevice, primitiveIdStaging.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, primitiveIdStaging.memory, nullptr);
		}
		std::cout << "total Vert : " << totalVert << "\n";
		std::cout << "total Vert Size : " << totalVertSize << "\n";
		std::cout << "total Index : " << totalIndex << "\n";
		std::cout << "total Index Size : " << totalIndexSize << "\n";
		std::cout << "total PrimitiveID : " << totalPrimitiveId << "\n";
		std::cout << "total PrimitiveID Size : " << totalPrimitiveIdSize << "\n";
	}

	void createSplittedPrimitiveIdsBuffer(VkQueue queue) {
		for (int i = 0; i < d_splittedPrimitiveIds.size(); i++) {
			h_splittedPrimitiveIdsDeviceAddress.push_back(getBufferDeviceAddress(d_splittedPrimitiveIds[i].buffer.buffer));
		}
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		uint32_t primitiveIdSize = static_cast<uint32_t>(d_splittedPrimitiveIds.size() * sizeof(uint64_t));
		
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			primitiveIdSize,
			&stagingBuffer,
			&stagingMemory,
			h_splittedPrimitiveIdsDeviceAddress.data()
		));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&d_splittedPrimitiveIdsDeviceAddress,
			primitiveIdSize,
			(void*)nullptr));

		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = {};

		copyRegion.size = primitiveIdSize;
		vkCmdCopyBuffer(copyCmd, stagingBuffer, d_splittedPrimitiveIdsDeviceAddress.buffer, 1, &copyRegion);

		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);
		vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, stagingMemory, nullptr);
	}

	/* create BLAS */
	void createAccelerationStructureBuffer(AccelerationStructure& accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
	{
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		VK_CHECK_RESULT(vkCreateBuffer(vulkanDevice->logicalDevice, &bufferCreateInfo, nullptr, &accelerationStructure.buffer));
		VkMemoryRequirements memoryRequirements{};
		vkGetBufferMemoryRequirements(vulkanDevice->logicalDevice, accelerationStructure.buffer, &memoryRequirements);
		VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
		memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
		memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
		VkMemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memoryAllocateInfo, nullptr, &accelerationStructure.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(vulkanDevice->logicalDevice, accelerationStructure.buffer, accelerationStructure.memory, 0));
	}

	uint64_t getBufferDeviceAddress(VkBuffer buffer)
	{
		VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
		bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bufferDeviceAI.buffer = buffer;
		return vkGetBufferDeviceAddressKHR(vulkanDevice->logicalDevice, &bufferDeviceAI);
	}

	ScratchBuffer createScratchBuffer(VkDeviceSize size)
	{
		ScratchBuffer scratchBuffer{};
		// Buffer and memory
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = size;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		VK_CHECK_RESULT(vkCreateBuffer(vulkanDevice->logicalDevice, &bufferCreateInfo, nullptr, &scratchBuffer.handle));
		VkMemoryRequirements memoryRequirements{};
		vkGetBufferMemoryRequirements(vulkanDevice->logicalDevice, scratchBuffer.handle, &memoryRequirements);
		VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
		memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
		memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
		VkMemoryAllocateInfo memoryAllocateInfo = {};
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memoryAllocateInfo, nullptr, &scratchBuffer.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(vulkanDevice->logicalDevice, scratchBuffer.handle, scratchBuffer.memory, 0));
		// Buffer device address
		VkBufferDeviceAddressInfoKHR bufferDeviceAddresInfo{};
		bufferDeviceAddresInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bufferDeviceAddresInfo.buffer = scratchBuffer.handle;
		scratchBuffer.deviceAddress = vkGetBufferDeviceAddressKHR(vulkanDevice->logicalDevice, &bufferDeviceAddresInfo);
		return scratchBuffer;
	}

	void deleteScratchBuffer(ScratchBuffer& scratchBuffer)
	{
		if (scratchBuffer.memory != VK_NULL_HANDLE) {
			vkFreeMemory(vulkanDevice->logicalDevice, scratchBuffer.memory, nullptr);
		}
		if (scratchBuffer.handle != VK_NULL_HANDLE) {
			vkDestroyBuffer(vulkanDevice->logicalDevice, scratchBuffer.handle, nullptr);
		}
	}

	void createBLAS(int cellIdx, VkQueue& queue) {
		VkAccelerationStructureGeometryKHR geometry{};
		VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo;
		VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo;

		uint32_t primitiveCount = d_splittedIndices[cellIdx].count / 3;
		VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

		vertexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(d_splittedVertices[cellIdx].buffer.buffer);
		indexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(d_splittedIndices[cellIdx].buffer.buffer);
		transformBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(tMatBuffer.buffer);

		geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		geometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
		geometry.geometry.triangles.maxVertex = d_splittedVertices[cellIdx].count;
		geometry.geometry.triangles.vertexStride = sizeof(float) * 3;
		geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
		geometry.geometry.triangles.indexData = indexBufferDeviceAddress;
		geometry.geometry.triangles.transformData = transformBufferDeviceAddress;

		buildRangeInfo.firstVertex = 0;
		buildRangeInfo.primitiveOffset = 0;
		buildRangeInfo.primitiveCount = d_splittedIndices[cellIdx].count / 3;
		buildRangeInfo.transformOffset = 0;
		pBuildRangeInfo = &buildRangeInfo;

		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &geometry;

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(
			vulkanDevice->logicalDevice,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			&primitiveCount,
			&accelerationStructureBuildSizesInfo);

		createAccelerationStructureBuffer(splittedBLAS[cellIdx], accelerationStructureBuildSizesInfo);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = splittedBLAS[cellIdx].buffer;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(vulkanDevice->logicalDevice, &accelerationStructureCreateInfo, nullptr, &splittedBLAS[cellIdx].handle);

		ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationStructureBuildGeometryInfo.dstAccelerationStructure = splittedBLAS[cellIdx].handle;
		accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationStructureBuildGeometryInfo,
			&pBuildRangeInfo);
		vulkanDevice->flushCommandBuffer(commandBuffer, queue);

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = splittedBLAS[cellIdx].handle;
		splittedBLAS[cellIdx].deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(vulkanDevice->logicalDevice, &accelerationDeviceAddressInfo);

		deleteScratchBuffer(scratchBuffer);
	}


	void createBLASes(VkQueue& queue) {
		glm::mat3x4 m = glm::dmat3x4(glm::mat4(1.0f));
		memcpy(&tMat, (void*)&m, sizeof(glm::mat3x4));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&tMatBuffer,
			sizeof(VkTransformMatrixKHR),
			&tMat));

		splittedBLAS.resize(d_splittedIndices.size());
		for (int cellIdx = 0; cellIdx < d_splittedIndices.size(); cellIdx++) {
			createBLAS(cellIdx, queue);
		}
	}

	void createTLAS(VkQueue& queue) {
		vector<VkAccelerationStructureInstanceKHR> instances;
		for (int i = 0; i < d_splittedIndices.size(); i++) {
			VkAccelerationStructureInstanceKHR instance{};
			instance.transform = tMat;
			instance.instanceCustomIndex = instances.size();
			instance.mask = 0xFF;
			instance.instanceShaderBindingTableRecordOffset = 0;
			instance.flags = VK_FLAGS_NONE;
			//instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			instance.accelerationStructureReference = splittedBLAS[i].deviceAddress;
			instances.push_back(instance);
		}

		vks::Buffer instancesBuffer;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&instancesBuffer,
			instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
			instances.data())
		);

		VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
		instanceDataDeviceAddress.deviceAddress = getBufferDeviceAddress(instancesBuffer.buffer);

		VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
		accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

		uint32_t primitive_count = instances.size();
		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(
			vulkanDevice->logicalDevice,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			&primitive_count,
			&accelerationStructureBuildSizesInfo);

		createAccelerationStructureBuffer(splittedTLAS, accelerationStructureBuildSizesInfo);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = splittedTLAS.buffer;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(vulkanDevice->logicalDevice, &accelerationStructureCreateInfo, nullptr, &splittedTLAS.handle);

		ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationBuildGeometryInfo.dstAccelerationStructure = splittedTLAS.handle;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;
		vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
		vulkanDevice->flushCommandBuffer(commandBuffer, queue);

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = splittedTLAS.handle;
		deleteScratchBuffer(scratchBuffer);
		instancesBuffer.destroy();
	}

public:
	vector<Attribute> d_splittedVertices;
	vector<Attribute> d_splittedIndices;
	vector<Attribute> d_splittedPrimitiveIds;
	vks::Buffer d_splittedPrimitiveIdsDeviceAddress;
	vector<AccelerationStructure> splittedBLAS;
	AccelerationStructure splittedTLAS;

	void init(vks::VulkanDevice* device) {
		this->vulkanDevice = device;
		this->gaussianCnt = gaussianCnt;
		vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(device->logicalDevice, "vkGetBufferDeviceAddressKHR"));
		vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device->logicalDevice, "vkCmdBuildAccelerationStructuresKHR"));
		vkBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device->logicalDevice, "vkBuildAccelerationStructuresKHR"));
		vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device->logicalDevice, "vkCreateAccelerationStructureKHR"));
		vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device->logicalDevice, "vkDestroyAccelerationStructureKHR"));
		vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device->logicalDevice, "vkGetAccelerationStructureBuildSizesKHR"));
		vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device->logicalDevice, "vkGetAccelerationStructureDeviceAddressKHR"));
	}

	void splitBlas(vks::Buffer& vertexBuffer, vks::Buffer& indexBuffer, VkQueue& queue) {
		copyDeviceToHost(vertexBuffer, indexBuffer, queue);

		assert(vertices.size() % 3 == 0);
		vector<glm::vec3> verticesVec3;
		for (int i = 0; i < vertices.size(); i += 3) {
			verticesVec3.push_back(glm::vec3(vertices[i], vertices[i + 1], vertices[i + 2]));
		}
		saveGeometries_SBLAS(verticesVec3, indices);

		h_splittedVertFP.resize(numCellsTotal);
		for (int i = 0; i < h_splittedVert.size(); i++) {
			for (int j = 0; j < h_splittedVert[i].size(); j++) {
				h_splittedVertFP[i].push_back(h_splittedVert[i][j].x);
				h_splittedVertFP[i].push_back(h_splittedVert[i][j].y);
				h_splittedVertFP[i].push_back(h_splittedVert[i][j].z);
			}
		}

		copyToDevice(queue);
		createSplittedPrimitiveIdsBuffer(queue);
	}

	void createAS(VkQueue& queue) {
		createBLASes(queue);
		createTLAS(queue);
	}

	~SplitBLAS() {
		for (int i = 0; i < d_splittedVertices.size(); i++) {
			d_splittedVertices[i].buffer.destroy();
		}
		for (int i = 0; i < d_splittedIndices.size(); i++) {
			d_splittedIndices[i].buffer.destroy();
		}
		for (int i = 0; i < d_splittedPrimitiveIds.size(); i++) {
			d_splittedPrimitiveIds[i].buffer.destroy();
		}
		d_splittedPrimitiveIdsDeviceAddress.destroy();
		for (int i = 0; i < splittedBLAS.size(); i++) {
			splittedBLAS[i].destroy(vulkanDevice->logicalDevice);
		}
		splittedTLAS.destroy(vulkanDevice->logicalDevice);
		tMatBuffer.destroy();
	}
};