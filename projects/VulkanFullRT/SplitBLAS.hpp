/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 */

#pragma once

#include "VulkanUtils.h"
#include "SimpleUtils.h"

#include <vector>

using namespace std;

class SplitBLAS {
private:
	vks::VulkanDevice* vulkanDevice;

	uint32_t gaussianCnt;
	glm::vec3 minPos{ FLT_MAX ,FLT_MAX ,FLT_MAX };
	glm::vec3 maxPos{ -FLT_MAX,-FLT_MAX,-FLT_MAX };

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
	void copyDeviceToHost(vks::Buffer& vertexBuffer, vks::Buffer& indexBuffer, VkQueue& queue);
	void saveGeometries_SBLAS(std::vector<glm::vec3>& vertexBuffer, std::vector<uint32_t>& indexBuffer);
	void copyToDevice(VkQueue& queue);
	void createSplittedPrimitiveIdsBuffer(VkQueue queue);
	/* create BLAS */
	void createAccelerationStructureBuffer(AccelerationStructure& accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);
	uint64_t getBufferDeviceAddress(VkBuffer buffer);
	ScratchBuffer createScratchBuffer(VkDeviceSize size);
	void deleteScratchBuffer(ScratchBuffer& scratchBuffer);
	void createBLAS(int cellIdx, VkQueue& queue);
	void createBLASes(VkQueue& queue);
	void createTLAS(VkQueue& queue);
	void destroyAS();

public:
	uint32_t cellsPerLongestAxis;

	vector<Attribute> d_splittedVertices;
	vector<Attribute> d_splittedIndices;
	vector<Attribute> d_splittedPrimitiveIds;
	vks::Buffer d_splittedPrimitiveIdsDeviceAddress;
	vector<AccelerationStructure> splittedBLAS;
	AccelerationStructure splittedTLAS;

	VkQueryPool ASBuildTimeStampQueryPool;
	std::vector<uint64_t> ASBuildTimeStamps;
	VkDeviceSize blasSize = 0;
	VkDeviceSize tlasSize = 0;

	~SplitBLAS();
	void init(vks::VulkanDevice* device);
	void splitBlas(vks::Buffer& vertexBuffer, vks::Buffer& indexBuffer, VkQueue& queue);
	void createAS(VkQueue& queue);
	void rebuild(vks::Buffer& vertexBuffer, vks::Buffer& indexBuffer, VkQueue& queue);
	void initASBuildTimestamp(VkQueue& queue);
	void printASBuildInfo(VkPhysicalDeviceProperties deviceProperties);
};