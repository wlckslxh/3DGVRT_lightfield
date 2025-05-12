#pragma once

#include "vulkan/vulkan.h"
#include "VulkanDevice.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace vk3DGRT {
	struct SplatSet {
		// standard poiont cloud attributes
		std::vector<float> positions; // point positions (x,y,z)
		// specific data fields introduced by INRIA for 3DGS
		std::vector<float> f_dc;      // 3 components per point (f_dc_0, f_dc_1, f_dc_2 in ply file)
		std::vector<float> f_rest;    // 45 components per point (f_rest_0 to f_rest_44 in ply file), SH coeficients
		std::vector<float> opacity;   // 1 value per point in ply file
		std::vector<float> scale;     // 3 components per point in ply file
		std::vector<float> rotation;  // 4 components per point in ply file - a quaternion

		// returns the number of splate in the set
		inline size_t size() const { return positions.size() / 3; }
	};

	struct GPrimitive {
		std::vector<float> gPrimVrt;	// Vertices of IcosaHedron Triangles
		std::vector<float> gPrimTri;	// Indicies of IcosaHedron Triangles
	};

	class PLYLoader {
	public:
		PLYLoader() {}
		~PLYLoader() {}

		bool loadPLYModel(std::string filename, SplatSet & output);
	};

	class Model {
	public:
		Model() {}
		~Model() {}

		SplatSet splatSet;		// Loaded 3DGRT model from .ply/.pt file

		struct Vertices {
			int count;
			VkBuffer buffer;
			VkDeviceMemory memory;
		} vertices;

		struct Indices {
			int count;
			VkBuffer buffer;
			VkDeviceMemory memory;
		} indices;

		void load3DGRTModel(std::string filename, vks::VulkanDevice* device);
		void createAndCopyDataToDevice(std::vector<float>& vertexBuffer, std::vector<uint32_t>& indexBuffer, VkQueue transferQueue, vks::VulkanDevice* vulkanDevice);
	};
}