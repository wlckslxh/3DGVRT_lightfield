/*
* Vulkan Utils
*
* Graphics Lab, Sogang University
*
*/

#pragma once

#include "VulkanglTFModel.h"
#include "../../base/Define.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vks {
	namespace utils {
		struct SpotLight {
			float innerConeAngle = 0.0f;
			float outerConeAngle = 0.0f;
			// need ray direction
		};

		struct Light {
			alignas(16) glm::vec3 position;
			alignas(4) float radius;
			alignas(16) glm::vec3 color;
			alignas(4) vkglTF::LightType type = vkglTF::LightType::LIGHT_TYPE_NONE;
		};

		struct UniformData {
			alignas(16) glm::mat4 viewInverse;
			alignas(16) glm::mat4 projInverse;
			alignas(16) Light lights[NUM_OF_DYNAMIC_LIGHTS];
		};

		struct UniformDataStaticLight {
			alignas(16) Light lights[NUM_OF_STATIC_LIGHTS];
		};

		void updateLightStaticInfo(UniformDataStaticLight& uniformDataStaticLight, BaseFrameObject& currentFrame, vkglTF::Model &scene, vks::VulkanDevice *vulkanDevice, VkQueue graphicsQueue);
		void updateLightDynamicInfo(UniformData& uniformData, vkglTF::Model& scene, float timer);
	}
}