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

	vector<vks::Buffer> splittedVertices;
	vector<vks::Buffer> splittedIndices;
	vector<vks::Buffer> splittedPrimitiveIds;

	vector<float> vertices;
	vector<uint32_t> indices;

	vector<vector<uint32_t>> primitiveIds;

public:
	void init(vks::VulkanDevice* device) {
		this->vulkanDevice = device;
		this->gaussianCnt = gaussianCnt;
	}

	void copyDeviceToHost(vks::Buffer& vertexBuffer, vks::Buffer& indexBuffer, VkQueue& queue) {
		vertices.resize(vertexBuffer.size);
		indices.resize(indexBuffer.size);

		/*** Vetices ***/
		vks::Buffer stagingBuffer;
		stagingBuffer.size = vertexBuffer.size;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.size, &stagingBuffer.buffer, &stagingBuffer.memory, nullptr));

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
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.size, &stagingBuffer.buffer, &stagingBuffer.memory, nullptr));

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

	void saveGeometries_SBLAS(std::vector<glm::vec3>& vertexBuffer, std::vector<uint32_t>& indexBuffer, VkQueue transferQueue) {
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
		const uint32_t numCellsTotal = numCells.x * numCells.y * numCells.z;

		std::vector<std::vector<glm::vec3>> splittedVert(numCellsTotal);
		std::vector<std::vector<uint32_t>> splittedIdx(numCellsTotal);
		primitiveIds.resize(numCellsTotal);

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
					// TODO : remove vertex redundancy
					if (uniqueVertices[cellIdx].count(polygon[0]) == 0) {
						uniqueVertices[cellIdx][polygon[0]] = static_cast<uint32_t>(splittedVert[cellIdx].size());
						splittedVert[cellIdx].push_back(polygon[0]);
					}
					splittedIdx[cellIdx].push_back(uniqueVertices[cellIdx][polygon[0]]);

					if (uniqueVertices[cellIdx].count(polygon[k]) == 0) {
						uniqueVertices[cellIdx][polygon[k]] = static_cast<uint32_t>(splittedVert[cellIdx].size());
						splittedVert[cellIdx].push_back(polygon[k]);
					}
					splittedIdx[cellIdx].push_back(uniqueVertices[cellIdx][polygon[k]]);

					if (uniqueVertices[cellIdx].count(polygon[k + 1]) == 0) {
						uniqueVertices[cellIdx][polygon[k + 1]] = static_cast<uint32_t>(splittedVert[cellIdx].size());
						splittedVert[cellIdx].push_back(polygon[k + 1]);
					}
					splittedIdx[cellIdx].push_back(uniqueVertices[cellIdx][polygon[k + 1]]);

					primitiveIds[cellIdx].push_back(primitiveId);
				}
			}
			primitiveId++;
		}

		//struct StagingBuffer {
		//	VkBuffer buffer;
		//	VkDeviceMemory memory;
		//};

		//int totalVert = 0;
		//int totalVertSize = 0;
		//int totalIndex = 0;
		//int totalIndexSize = 0;
		//for (int i = 0; i < numCellsTotal; i++) {

		//	size_t vertexBufferSize = splittedVert[i].size() * sizeof(glm::vec3);
		//	size_t indexBufferSize = splittedIdx[i].size() * sizeof(uint32_t);
		//	if (vertexBufferSize == 0) continue;

		//	Vertices cellVertices;
		//	Indices cellIndices;
		//	cellVertices.count = static_cast<uint32_t>(splittedVert[i].size());
		//	cellIndices.count = static_cast<uint32_t>(splittedIdx[i].size());
		//	totalVert += cellVertices.count;
		//	totalVertSize += vertexBufferSize;
		//	totalIndex += cellIndices.count;
		//	totalIndexSize += indexBufferSize;

		//	//assert((vertexBufferSize > 0) && (indexBufferSize > 0));

		//	StagingBuffer vertexStaging, indexStaging;

		//	// Vertex
		//	VK_CHECK_RESULT(device->createBuffer(
		//		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		//		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		//		vertexBufferSize,
		//		&vertexStaging.buffer,
		//		&vertexStaging.memory,
		//		splittedVert[i].data()));
		//	// Index
		//	VK_CHECK_RESULT(device->createBuffer(
		//		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		//		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		//		indexBufferSize,
		//		&indexStaging.buffer,
		//		&indexStaging.memory,
		//		splittedIdx[i].data()));

		//	// Create device local buffers
		//	// Vertex buffer
		//	VK_CHECK_RESULT(device->createBuffer(
		//		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsageFlags,
		//		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		//		vertexBufferSize,
		//		&cellVertices.buffer,
		//		&cellVertices.memory));
		//	// Index buffer
		//	VK_CHECK_RESULT(device->createBuffer(
		//		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsageFlags,
		//		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		//		indexBufferSize,
		//		&cellIndices.buffer,
		//		&cellIndices.memory));

		//	// Copy from staging buffers
		//	VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		//	VkBufferCopy copyRegion = {};

		//	copyRegion.size = vertexBufferSize;
		//	vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, cellVertices.buffer, 1, &copyRegion);

		//	copyRegion.size = indexBufferSize;
		//	vkCmdCopyBuffer(copyCmd, indexStaging.buffer, cellIndices.buffer, 1, &copyRegion);

		//	splittedVertices.push_back(cellVertices);
		//	splittedIndices.push_back(cellIndices);

		//	device->flushCommandBuffer(copyCmd, transferQueue, true);

		//	vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
		//	vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
		//	vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
		//	vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);
		//}
		//std::cout << "total Vert : " << totalVert << "\n";
		//std::cout << "total Vert Size : " << totalVertSize << "\n";
		//std::cout << "total Index : " << totalIndex << "\n";
		//std::cout << "total Index Size : " << totalIndexSize << "\n";
	}

	void splitBlas(vks::Buffer& vertexBuffer, vks::Buffer& indexBuffer, VkQueue& queue) {
		copyDeviceToHost(vertexBuffer, indexBuffer, queue);

		assert(vertices.size() % 3 == 0);
		vector<glm::vec3> verticesVec3;
		for (int i = 0; i < vertices.size(); i += 3) {
			verticesVec3.push_back(glm::vec3(vertices[i], vertices[i + 1], vertices[i + 2]));
		}
		saveGeometries_SBLAS(verticesVec3, indices, queue);
	}
};