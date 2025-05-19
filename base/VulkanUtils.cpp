#include "VulkanUtils.h"

namespace vks {
	namespace utils {
		void updateLightStaticInfo(UniformDataStatic& uniformDataStaticLight, BaseFrameObject& currentFrame, vkglTF::Model &scene, vks::VulkanDevice *vulkanDevice, VkQueue graphicsQueue)
		{
			for (uint32_t i = STATIC_LIGHT_OFFSET; i < NUM_OF_LIGHTS_SUPPORTED; i++) {
				uniformDataStaticLight.lights[i - STATIC_LIGHT_OFFSET].position = scene.lights[i].matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
				uniformDataStaticLight.lights[i - STATIC_LIGHT_OFFSET].color = scene.lights[i].color;
				uniformDataStaticLight.lights[i - STATIC_LIGHT_OFFSET].type = scene.lights[i].type;
				uniformDataStaticLight.lights[i - STATIC_LIGHT_OFFSET].radius = scene.lights[i].radius;
			}

			// mapping
			vks::Buffer stagingBuffer;
			stagingBuffer.size = sizeof(UniformDataStatic);
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.size, &stagingBuffer.buffer, &stagingBuffer.memory, nullptr));

			void* data;
			vkMapMemory(vulkanDevice->logicalDevice, stagingBuffer.memory, 0, sizeof(UniformDataStatic), 0, &data);
			memcpy(data, (void*)&uniformDataStaticLight, sizeof(uniformDataStaticLight));
			vkUnmapMemory(vulkanDevice->logicalDevice, stagingBuffer.memory);

			VkBufferCopy copyRegion;
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			copyRegion.size = stagingBuffer.size;
			vulkanDevice->copyBuffer(&stagingBuffer, &currentFrame.uniformBufferStatic, graphicsQueue, &copyRegion);

			vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, stagingBuffer.memory, nullptr);
		}

		void updateLightDynamicInfo(UniformDataDynamic& uniformData, vkglTF::Model &scene, float timer)
		{
			for (uint32_t i = 0; i < STATIC_LIGHT_OFFSET; i++) {
//				// light position
//				glm::vec4 lightPos = scene.lights[i].matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
//
//#if ASSET != 1
//				uniformData.lights[i].position = glm::vec4(lightPos.x + cos(glm::radians(timer * 360.0f)) * 30.0f, lightPos.y, lightPos.z + sin(glm::radians(timer * 360.0f)) * 30.0f, 1.0f);
//#elif ASSET == 1
//				uniformData.lights[i].position = glm::vec4(lightPos.x + cos(glm::radians(timer * 360.0f)) * 25.0f, lightPos.y, lightPos.z + sin(glm::radians(timer * 360.0f)) * 5.0f, 1.0f);
//#endif
//
//				// the rest
//				uniformData.lights[i].color = scene.lights[i].color;
//				uniformData.lights[i].type = scene.lights[i].type;
//				uniformData.lights[i].radius = scene.lights[i].radius;
			}
		}

		void updateUniformBufferStatic(UniformDataStatic& params, BaseFrameObject& currentFrame, vks::VulkanDevice* vulkanDevice, VkQueue queue) {
			// mapping
			vks::Buffer stagingBuffer;
			stagingBuffer.size = sizeof(UniformDataStatic);
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.size, &stagingBuffer.buffer, &stagingBuffer.memory, nullptr));

			void* data;
			vkMapMemory(vulkanDevice->logicalDevice, stagingBuffer.memory, 0, sizeof(UniformDataStatic), 0, &data);
			memcpy(data, (void*)&params, sizeof(params));
			vkUnmapMemory(vulkanDevice->logicalDevice, stagingBuffer.memory);

			VkBufferCopy copyRegion;
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			copyRegion.size = stagingBuffer.size;
			vulkanDevice->copyBuffer(&stagingBuffer, &currentFrame.uniformBufferStatic, queue, &copyRegion);

			vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, stagingBuffer.memory, nullptr);
		}
	}
}

