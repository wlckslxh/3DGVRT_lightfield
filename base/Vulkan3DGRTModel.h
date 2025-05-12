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

		//// standard poiont cloud attributes
		//std::vector<glm::vec3> positions; // point positions (x,y,z)
		//// specific data fields introduced by INRIA for 3DGS
		//std::vector<glm::vec3> f_dc;      // 3 components per point (f_dc_0, f_dc_1, f_dc_2 in ply file)
		//std::vector<float> f_rest;    // 45 components per point (f_rest_0 to f_rest_44 in ply file), SH coeficients
		//std::vector<float> opacity;   // 1 value per point in ply file
		//std::vector<glm::vec3> scale;     // 3 components per point in ply file
		//std::vector<glm::vec4> rotation;  // 4 components per point in ply file - a quaternion

		// returns the number of splate in the set
		//inline size_t size() const { return positions.size() / 3; }
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

		SplatSet splatSet;

		void load3DGRTModel(std::string filename, vks::VulkanDevice* device);
	};
}