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

		struct Aabb {
			float minX, minY, minZ;
			float maxX, maxY, maxZ;
		};

		struct UniformDataDynamic {
			alignas(16) glm::mat4 viewInverse;
			alignas(16) glm::mat4 projInverse;
			//alignas(16) Light lights[NUM_OF_DYNAMIC_LIGHTS];
			// alignas(16) Params3DGRT params;
		};

		enum MOGRenderOpts {
			MOGRenderNone = 0,
			MOGRenderAdaptiveKernelClamping = 1 << 0,
			MOGRenderWithNormals = 1 << 1,
			MOGRenderWithHitCounts = 1 << 2,
			MOGRenderDefault = MOGRenderNone
		};

		struct UniformDataStatic {
			alignas(16) Light lights[NUM_OF_STATIC_LIGHTS];
			/* 3DGRT */
			alignas(16) Aabb aabb = { -100.0f, -100.0f, -100.0f, 100.0f, 100.0f, 100.0f };
			alignas(16) float minTransmittance = 0.001f; // to be separated to Config.h?
			alignas(4) float hitMinGaussianResponse = 0.0113f;	// particle kernel min response. to be separated to Config.h?
			alignas(4) unsigned int sphEvalDegree = 3;	// n active features. to be separated to Config.h?
#if BUFFER_REFERENCE
			uint64_t densityBufferDeviceAddress;
			uint64_t sphCoefficientBufferDeviceAddress;
#endif
		};

		struct GaussianEnclosingUniformData {
			alignas(4) unsigned int numOfGaussians;
			alignas(4) float kernelMinResponse;
			alignas(4) unsigned int opts;
			alignas(4) float degree;
		};

		void updateLightStaticInfo(UniformDataStatic& uniformDataStaticLight, BaseFrameObject& currentFrame, vkglTF::Model &scene, vks::VulkanDevice *vulkanDevice, VkQueue graphicsQueue);
		void updateLightDynamicInfo(UniformDataDynamic& uniformData, vkglTF::Model& scene, float timer);
		void updateUniformBufferStatic(UniformDataStatic& params, BaseFrameObject& currentFrame, vks::VulkanDevice* vulkanDevice, VkQueue queue);
	}
}