#include "Vulkan3DGRTModel.h"
//#include "torch/script.h"
#include "miniply.h"
#include "chrono"

namespace vk3DGRT {
	bool PLYLoader::loadPLYModel(std::string filename, SplatSet & output)
	{
		auto startTime = std::chrono::high_resolution_clock::now();

		// Open the file
		miniply::PLYReader reader(filename.c_str());
		if (!reader.valid())
		{
			std::cout << "Error: ply loader failed to open file: " << filename << std::endl;
			return false;
		}

		uint32_t indices[45];
		bool gsFound = false;

		while (reader.has_element() && !gsFound)
		{
			if (reader.element_is(miniply::kPLYVertexElement) && reader.load_element())
			{
				const uint32_t numVerts = reader.num_rows();
				if (numVerts == 0)
				{
					std::cout << "Warning: ply loader skipping empty ply element " << std::endl;
					continue;  // move to next while iteration
				}
				output.positions.resize(numVerts * 3);
				output.scale.resize(numVerts * 3);
				output.rotation.resize(numVerts * 4);
				output.opacity.resize(numVerts);
				output.f_dc.resize(numVerts * 3);
				output.f_rest.resize(numVerts * 45);
				// load progress
				const uint32_t total = numVerts * (3 + 3 + 4 + 1 + 3 + 45);
				uint32_t       loaded = 0;

				// put that first so the loading progress looks better
				if (reader.find_properties(indices, 45, "f_rest_0", "f_rest_1", "f_rest_2", "f_rest_3", "f_rest_4", "f_rest_5",
					"f_rest_6", "f_rest_7", "f_rest_8", "f_rest_9", "f_rest_10", "f_rest_11", "f_rest_12",
					"f_rest_13", "f_rest_14", "f_rest_15", "f_rest_16", "f_rest_17", "f_rest_18",
					"f_rest_19", "f_rest_20", "f_rest_21", "f_rest_22", "f_rest_23", "f_rest_24",
					"f_rest_25", "f_rest_26", "f_rest_27", "f_rest_28", "f_rest_29", "f_rest_30", "f_rest_31",
					"f_rest_32", "f_rest_33", "f_rest_34", "f_rest_35", "f_rest_36", "f_rest_37", "f_rest_38",
					"f_rest_39", "f_rest_40", "f_rest_41", "f_rest_42", "f_rest_43", "f_rest_44"))
				{
					reader.extract_properties(indices, 45, miniply::PLYPropertyType::Float, output.f_rest.data());
					loaded += numVerts * 45;
				}
				if (reader.find_properties(indices, 3, "x", "y", "z"))
				{
					reader.extract_properties(indices, 3, miniply::PLYPropertyType::Float, output.positions.data());
					loaded += numVerts * 3;
				}
				if (reader.find_properties(indices, 1, "opacity"))
				{
					reader.extract_properties(indices, 1, miniply::PLYPropertyType::Float, output.opacity.data());
					loaded += numVerts;
				}
				if (reader.find_properties(indices, 3, "scale_0", "scale_1", "scale_2"))
				{
					reader.extract_properties(indices, 3, miniply::PLYPropertyType::Float, output.scale.data());
					loaded += numVerts * 3;
				}
				if (reader.find_properties(indices, 4, "rot_0", "rot_1", "rot_2", "rot_3"))
				{
					reader.extract_properties(indices, 4, miniply::PLYPropertyType::Float, output.rotation.data());
					loaded += numVerts * 4;
				}
				if (reader.find_properties(indices, 3, "f_dc_0", "f_dc_1", "f_dc_2"))
				{
					reader.extract_properties(indices, 3, miniply::PLYPropertyType::Float, output.f_dc.data());
					loaded += numVerts * 3;
				}


				gsFound = true;
			}

			reader.next_element();
		}

		if (gsFound)
		{
			auto      endTime = std::chrono::high_resolution_clock::now();
			long long loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
			std::cout << "File loaded in " << loadTime << "ms" << std::endl;
		}
		else
		{
			std::cout << "Error: invalid 3DGS PLY file" << std::endl;
		}

		return gsFound;
	}

	void Model::load3DGRTModel(std::string filename, vks::VulkanDevice* device)
	{
		if (filename.find_last_of(".") != std::string::npos) {

			// Can't load .pt file in C++ since the model file is exported using pickle module
			// 
			//if (filename.substr(filename.find_last_of(".") + 1) == "pt")	// Case 0: checkpoint.pt file
			//{
			//
			//	try {
			//		std::filesystem::path path = std::filesystem::current_path() / "ckpt_last.pt";
			//		torch::jit::script::Module module = torch::jit::load(path.string());
			//	}
			//	catch (const c10::Error& e) {
			//		std::cerr << e.what() << std::endl;
			//		vks::tools::exitFatal("Error loading the model: \"" + filename, -1);
			//		return;
			//	}
			//}
			if (filename.substr(filename.find_last_of(".") + 1) == "ply") // .ply file
			{
				PLYLoader plyLoader;
				plyLoader.loadPLYModel(filename, splatSet);
				std::cout << "done" << std::endl;
			}
			else
			{
				vks::tools::exitFatal("Unsupported Extensnion: \"" + filename.substr(filename.find_last_of(".") + 1), -1);
				return;
			}
		}
		else
		{
			vks::tools::exitFatal("Extension not found", -1);
		}
	}

	void Model::allocateAttributeBuffers(vks::VulkanDevice* vulkanDevice, VkQueue queue)
	{
		vertices.count = 3 * 12 * splatSet.size();
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&vertices.storageBuffer,
			sizeof(float) * vertices.count));

		indices.count = 3 * 20 * splatSet.size();
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&indices.storageBuffer,
			sizeof(float) * indices.count))

		positions.count = 3 * splatSet.size();
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&positions.storageBuffer,
			sizeof(float) * positions.count));
		vulkanDevice->copyBuffer(splatSet.positions.data(), &positions.storageBuffer, queue);

		rotations.count = 4 * splatSet.size();
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&rotations.storageBuffer,
			sizeof(float) * rotations.count));
		vulkanDevice->copyBuffer(splatSet.rotation.data(), &rotations.storageBuffer, queue);

		scales.count = 3 * splatSet.size();
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&scales.storageBuffer,
			sizeof(float) * scales.count));
		vulkanDevice->copyBuffer(splatSet.scale.data(), &scales.storageBuffer, queue);

		densities.count = splatSet.size();
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&densities.storageBuffer,
			sizeof(float) * densities.count));
		vulkanDevice->copyBuffer(splatSet.opacity.data(), &densities.storageBuffer, queue);
	}
}

