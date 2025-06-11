/*
 * Vulkan device class
 *
 * Encapsulates a physical Vulkan device and its logical representation
 *
 * Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#pragma once

#include "VulkanBuffer.h"
#include "VulkanTools.h"
#include "vulkan/vulkan.h"
#include <algorithm>
#include <assert.h>
#include <exception>

#include <map>
#include <optional>
#include <utility>
#include <set>

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

namespace sg {
	//QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice);
	//SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice);
	bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice, std::vector<const char*>);
	//bool isDeviceSuitable(VkPhysicalDevice physicalDevice);
	//int rateDeviceSuitability(VkPhysicalDevice physicalDevice);
	//VkSampleCountFlagBits getMaxUsableSampleCount();
	//void pickPhysicalDevice();
};

namespace vks
{

struct VulkanDevice
{
	/** @brief Physical device representation */
	VkPhysicalDevice physicalDevice;
	/** @brief Logical device representation (application's view of the device) */
	VkDevice logicalDevice;
	/** @brief Properties of the physical device including limits that the application can check against */
	VkPhysicalDeviceProperties properties;
	/** @brief Features of the physical device that an application can use to check if a feature is supported */
	VkPhysicalDeviceFeatures features;
	/** @brief Features that have been enabled for use on the physical device */
	VkPhysicalDeviceFeatures enabledFeatures;
	/** @brief Memory types and heaps of the physical device */
	VkPhysicalDeviceMemoryProperties memoryProperties;
	/** @brief Queue family properties of the physical device */
	std::vector<VkQueueFamilyProperties> queueFamilyProperties;
	/** @brief List of extensions supported by the device */
	std::vector<std::string> supportedExtensions;
	/** @brief Default command pool for the graphics queue family index */
	VkCommandPool commandPool = VK_NULL_HANDLE;
	/** @brief Contains queue family indices */
	struct
	{
		uint32_t graphics;
		uint32_t compute;
		uint32_t transfer;
	} queueFamilyIndices;
	operator VkDevice() const
	{
		return logicalDevice;
	};
	explicit VulkanDevice(VkPhysicalDevice physicalDevice);
	~VulkanDevice();
	uint32_t        getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound = nullptr) const;
	uint32_t        getQueueFamilyIndex(VkQueueFlags queueFlags) const;
	VkResult        createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> enabledExtensions, void *pNextChain, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
	VkResult        createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data = nullptr);
	VkResult        createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer *buffer, VkDeviceSize size, void *data = nullptr);
    VkResult		createAndMapBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer* buffer, VkDeviceSize size, void* data);
	VkCommandPool   createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	// basic version
	void copyBuffer(vks::Buffer* src, vks::Buffer* dst, VkQueue queue, VkBufferCopy* copyRegion = nullptr);
	VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin = false);
	VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = false);
	// extended version for using flags of command buffer
	void copyBuffer(vks::Buffer* src, vks::Buffer* dst, VkQueue queue, VkCommandBufferUsageFlagBits flags, VkBufferCopy* copyRegion = nullptr);
	void copyBuffer(void* data, vks::Buffer* dst, VkQueue queue, VkBufferCopy* copyRegion = nullptr);
	void copyImageToBuffer(VkImage srcImg, vks::Buffer dstBuf, VkQueue queue, VkImageLayout imgLayout, uint32_t width, uint32_t height);
	VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin, VkCommandBufferUsageFlagBits flags);
	VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin, VkCommandBufferUsageFlagBits flags);

	void            flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true);
	void            flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);
	bool            extensionSupported(std::string extension);
	VkFormat        getSupportedDepthFormat(bool checkSamplingSupport);

	void createAndCopyToDeviceBuffer(void* data, VkBuffer* buffer, VkDeviceMemory* memory, size_t bufferSize, VkQueue queue, VkBufferUsageFlags usageFlags = 0x0, VkMemoryPropertyFlags memoryFlags = 0x0);
	void createAndCopyToDeviceBuffer(void* data, vks::Buffer& buffer, size_t bufferSize, VkQueue queue, VkBufferUsageFlags usageFlags = 0x0, VkMemoryPropertyFlags memoryFlags = 0x0);
};
}        // namespace vks
