/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 * 
 * Full Ray Tracing
 * 
 * Pipeline Composition
 * |
 * |------ (1) Gaussian Enclosing pass : Vulkan Compute pipeline
 * |
 * |------ (2) Particle Rendering pass : Vulkan Ray tracing pipeline or Vulkan Compute pipeline
 */

#include "VulkanRTCommon.h"
#include "VulkanUtils.h"
#include "SimpleUtils.h"
#include "Vulkan3DGRTModel.h"

#if SPLIT_BLAS && !RAY_QUERY
#include "SplitBLAS.hpp"
#endif

#if EVAL_QUALITY
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

#define DIR_PATH "VulkanFullRT/"

class VulkanFullRT : public VulkanRTCommon
{
public:

	struct ParticleDensity {
		glm::vec3 position;
		float density;
		glm::vec4 quaternion;
		glm::vec3 scale;
		float padding;
	};
	
	struct ParticleSphCoefficient {
		glm::vec3 featuresAlbedo;
		float featuresSpecular[SPECULAR_DIMENSION];
	};

	vks::Buffer particleDensities;	//read only
	vks::Buffer particleSphCoefficients;	//read only

	// For 3DGRT Model
	AccelerationStructure bottomLevelAS3DGRT{};
	AccelerationStructure topLevelAS3DGRT{};

#if SPLIT_BLAS && !RAY_QUERY
	SplitBLAS splitBLAS;
#endif

	vks::Buffer transformBuffer3DGRT;

#if !RAY_QUERY
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};
	struct ShaderBindingTables {
		ShaderBindingTable raygen;
		ShaderBindingTable miss;
		ShaderBindingTable hit;
	} shaderBindingTables;
#endif

	vks::utils::UniformDataDynamic uniformDataDynamic;
	vks::utils::UniformDataStatic uniformDataStatic;
	vks::utils::GaussianEnclosingUniformData gaussianEnclosingUniformData;

	struct SpecializationData {
		uint32_t numOfLights = NUM_OF_LIGHTS_SUPPORTED;
		uint32_t numOfDynamicLights = NUM_OF_DYNAMIC_LIGHTS;
		uint32_t numOfStaticLights = NUM_OF_STATIC_LIGHTS;
		uint32_t staticLightOffset = STATIC_LIGHT_OFFSET;
		uint32_t windowSizeX = 1;
		uint32_t windowSizeY = 1;
	} specializationData;

	// for Particle Rendering pass
	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

	struct GaussianEnclosing {
		VkPipeline pipeline{ VK_NULL_HANDLE };
		VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
		VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
		
		vks::Buffer uniformBuffer;
		vks::Buffer totalCounts;
		VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };
	} gaussianEnclosing;

	vk3DGRT::Model gModel;

	struct FrameObject : public BaseFrameObject {
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
#if ENABLE_HIT_COUNTS && !RAY_QUERY
		vks::Buffer hitCountsbuffer;
#endif
	};

	std::vector<FrameObject> frameObjects;
	std::vector<BaseFrameObject*> pBaseFrameObjects;

	VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeatures{};
	VkPhysicalDeviceHostQueryResetFeaturesEXT physicalDeviceHostQueryResetFeatures{};
#if RAY_QUERY
	VkPhysicalDeviceRayQueryFeaturesKHR enabledRayQueryFeatures{};
#endif

	const uint32_t timeStampCountPerFrame = 1;
#if !SPLIT_BLAS
	VkQueryPool ASBuildTimeStampQueryPool;
	std::vector<uint64_t> ASBuildTimeStamps;
	VkDeviceSize blasSize;
	VkDeviceSize tlasSize;
#endif

#if LOAD_GLTF
	// For Triangle Mesh (Should be renamed)
	AccelerationStructure bottomLevelAS{};
	AccelerationStructure topLevelAS{};

	struct GeometryNode {
		uint64_t vertexBufferDeviceAddress;
		uint64_t indexBufferDeviceAddress;
		int32_t textureIndexBaseColor;
		int32_t textureIndexOcclusion;
		int32_t textureIndexNormal;
		int32_t textureIndexMetallicRoughness;
		int32_t textureIndexEmissive;
		float reflectance;
		float refractance;
		float ior;
		float metallicFactor;
		float roughnessFactor;
	};
	vks::Buffer geometryNodesBuffer;

	vks::Buffer transformBuffer;

	vkglTF::Model scene;
#endif

#if GAUSSIAN_LIGHT_FIELD
	struct GaussianSampledSphere {
		alignas(16) glm::vec3 center;
		float radius;
	};

	struct GaussianLightField {
		VkPipeline pipeline{ VK_NULL_HANDLE };
		VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
		VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

		vks::Buffer uniformBufferStatic;
		vks::Buffer rayDirBuffer;
		vks::Buffer rayPosBuffer;
		vector<glm::vec2> rayPos;
		vector<glm::vec2> rayDir;

		VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };

		VkImage image;
		VkImageView imageView;
		VkDeviceMemory imageMemory;

		const unsigned int sphereRedivisionLevel = SUBDIVISION_LEVEL;
		unsigned int samplingCameraNum;
		unsigned int sampleImageWidth;
		unsigned int sampleImageHeight;

		std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};
		struct ShaderBindingTables {
			ShaderBindingTable raygen;
			ShaderBindingTable miss;
			ShaderBindingTable hit;
		} shaderBindingTables;

		struct UniformDataStatic {
			/* 3DGRT */
			alignas(16) Aabb aabb = { -100.0f, -100.0f, -100.0f, 100.0f, 100.0f, 100.0f };
			alignas(16) float minTransmittance = 0.001f; // to be separated to Config.h?
			alignas(16) glm::vec4 sphereInfo;
			alignas(4) float hitMinGaussianResponse = 0.0113f;	// particle kernel min response. to be separated to Config.h?
			alignas(4) unsigned int sphEvalDegree = 3;	// n active features. to be separated to Config.h?
			alignas(4) unsigned int level;
			alignas(4) unsigned int width = SAMPLED_CAMERA_NUM_WIDTH;
			alignas(4) unsigned int height = SAMPLED_CAMERA_NUM_HEIGHT;
#if BUFFER_REFERENCE
			uint64_t densityBufferDeviceAddress;
			uint64_t sphCoefficientBufferDeviceAddress;
#endif
		}uniformDataStatic;

		//for ray tracing sampled sphere
		VkAabbPositionsKHR gaussianSampledSphereAABB;
		vks::Buffer gaussianSampledSphereBuffer;
		vks::Buffer aabbBuffer;
		GaussianSampledSphere gaussianSampledSphere;
		AccelerationStructure gaussianSampledBLAS{};
		AccelerationStructure gaussianSampledTLAS{};
	} gaussianLightField;

	struct TriangleIdx {
		int v1, v2, v3;
		string label;
		TriangleIdx(const int& x, const int& y, const int& z, const string& c) {
			v1 = x;
			v2 = y;
			v3 = z;
			label = c;
		};
	};

	//for checking redundancy of sphere redivision vertices
	struct Vec3Hash {
		size_t operator()(const glm::vec3& v) const noexcept {
			auto h1 = std::hash<float>()(v.x);
			auto h2 = std::hash<float>()(v.y);
			auto h3 = std::hash<float>()(v.z);
			return h1 ^ (h2 << 1) ^ (h3 << 2);
		}
	};
	struct Vec3Eq {
		bool operator()(const glm::vec3& a, const glm::vec3& b) const noexcept {
			return glm::length(a - b) < 1e-6f; // epsilon 비교
		}
	};

	std::unordered_map<glm::vec3, int, Vec3Hash, Vec3Eq> vertexMap;
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	typedef unsigned char byte;
#endif

	VulkanFullRT() : VulkanRTCommon()
	{
		title = "Abura Soba - Vulkan Full Ray Tracing";

#if RAY_QUERY
		rayQueryOnly = true;
#endif
	}

	~VulkanFullRT()
	{
		if (device) {
			// for Gaussian Enclosing pass
			vkDestroyPipeline(device, gaussianEnclosing.pipeline, nullptr);
			vkDestroyPipelineLayout(device, gaussianEnclosing.pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, gaussianEnclosing.descriptorSetLayout, nullptr);

			// for Particle Rendering pass
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

			for (FrameObject& frame : frameObjects)
			{
				frame.uniformBuffer.destroy();
				frame.uniformBufferStatic.destroy();

				vkDestroyQueryPool(device, frame.timeStampQueryPool, nullptr);
			}

			particleDensities.destroy();
			particleSphCoefficients.destroy();

			gaussianEnclosing.uniformBuffer.destroy();
			
			deleteAccelerationStructure(bottomLevelAS3DGRT);
			deleteAccelerationStructure(topLevelAS3DGRT);
			transformBuffer3DGRT.destroy();
		
#if !RAY_QUERY
			shaderBindingTables.raygen.destroy();
			shaderBindingTables.miss.destroy();
			shaderBindingTables.hit.destroy();
#endif
			destroyCommandBuffers();

#if LOAD_GLTF
			geometryNodesBuffer.destroy();
			deleteAccelerationStructure(bottomLevelAS);
			deleteAccelerationStructure(topLevelAS);
			transformBuffer.destroy();
#endif

#if GAUSSIAN_LIGHT_FIELD
			vkDestroyImageView(device, gaussianLightField.imageView, nullptr);
			vkDestroyImage(device, gaussianLightField.image, nullptr);
			vkFreeMemory(device, gaussianLightField.imageMemory, nullptr);
#endif
		}
	}

	virtual void createCommandBuffers()
	{
		VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

		for (FrameObject& frame : frameObjects)
		{
			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &frame.commandBuffer));
		}
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &gaussianEnclosing.commandBuffer));
#if GAUSSIAN_LIGHT_FIELD
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &gaussianLightField.commandBuffer));
#endif
	}

	virtual void destroyCommandBuffers()
	{
		for (FrameObject& frame : frameObjects)
		{
			vkFreeCommandBuffers(device, cmdPool, 1, &frame.commandBuffer);
		}
		vkFreeCommandBuffers(device, cmdPool, 1, &gaussianEnclosing.commandBuffer);
	}


	void createAccelerationStructureBuffer(AccelerationStructure& accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
	{
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &accelerationStructure.buffer));
		VkMemoryRequirements memoryRequirements{};
		vkGetBufferMemoryRequirements(device, accelerationStructure.buffer, &memoryRequirements);
		VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
		memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
		memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
		VkMemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &accelerationStructure.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, accelerationStructure.buffer, accelerationStructure.memory, 0));
	}

#if LOAD_GLTF
	/*
	Create the bottom level acceleration structure that contains the scene's actual geometry (vertices, triangles)
	*/
	void createBottomLevelAccelerationStructure()
	{
		// Use transform matrices from the glTF nodes
		std::vector<VkTransformMatrixKHR> transformMatrices{};
		for (auto node : scene.linearNodes) {
			if (node->mesh) {
				for (auto primitive : node->mesh->primitives) {
					if (primitive->indexCount > 0) {
						VkTransformMatrixKHR transformMatrix{};
						//auto m = glm::mat3x4(glm::transpose(node->getMatrix()));
						auto m = glm::mat3x4(glm::mat4(1.0f));	// The code above doesn't need because of pretransform.
						memcpy(&transformMatrix, (void*)&m, sizeof(glm::mat3x4));
						transformMatrices.push_back(transformMatrix);
					}
				}
			}
		}

		// Transform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&transformBuffer,
			static_cast<uint32_t>(transformMatrices.size()) * sizeof(VkTransformMatrixKHR),
			transformMatrices.data()));

		// Build
		// One geometry per glTF node, so we can index materials using gl_GeometryIndexEXT
		std::vector<uint32_t> maxPrimitiveCounts;
		std::vector<VkAccelerationStructureGeometryKHR> geometries;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> pBuildRangeInfos;
		std::vector<GeometryNode> geometryNodes;

		for (auto node : scene.linearNodes) {
			if (node->mesh) {
				for (auto primitive : node->mesh->primitives) {
					if (primitive->indexCount > 0) {
						VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
						VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
						VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

						vertexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(scene.vertices.buffer);// +primitive->firstVertex * sizeof(vkglTF::Vertex);
						indexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(scene.indices.buffer) + primitive->firstIndex * sizeof(uint32_t);
						transformBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(transformBuffer.buffer) + static_cast<uint32_t>(geometryNodes.size()) * sizeof(VkTransformMatrixKHR);

						VkAccelerationStructureGeometryKHR geometry{};
						geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
						geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
						geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
						geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
						geometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
						geometry.geometry.triangles.maxVertex = scene.vertices.count;
						geometry.geometry.triangles.vertexStride = sizeof(vkglTF::Vertex);
						geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
						geometry.geometry.triangles.indexData = indexBufferDeviceAddress;
						geometry.geometry.triangles.transformData = transformBufferDeviceAddress;
						geometries.push_back(geometry);
						maxPrimitiveCounts.push_back(primitive->indexCount / 3);

						VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
						buildRangeInfo.firstVertex = 0;
						buildRangeInfo.primitiveOffset = 0; // primitive->firstIndex * sizeof(uint32_t);
						buildRangeInfo.primitiveCount = primitive->indexCount / 3;
						buildRangeInfo.transformOffset = 0;
						buildRangeInfos.push_back(buildRangeInfo);

						GeometryNode geometryNode{};
						geometryNode.vertexBufferDeviceAddress = vertexBufferDeviceAddress.deviceAddress;
						geometryNode.indexBufferDeviceAddress = indexBufferDeviceAddress.deviceAddress;
						geometryNode.textureIndexBaseColor = primitive->material.baseColorTexture ? primitive->material.baseColorTexture->index : -1;
						geometryNode.textureIndexOcclusion = primitive->material.occlusionTexture ? primitive->material.occlusionTexture->index : -1;
						geometryNode.textureIndexNormal = primitive->material.normalTexture ? primitive->material.normalTexture->index : -1;
						geometryNode.textureIndexMetallicRoughness = primitive->material.metallicRoughnessTexture ? primitive->material.metallicRoughnessTexture->index : -1;
						geometryNode.textureIndexEmissive = primitive->material.emissiveTexture ? primitive->material.emissiveTexture->index : -1;
						geometryNode.reflectance = primitive->material.Kr;
						geometryNode.refractance = primitive->material.Kt;
						geometryNode.ior = primitive->material.ior;
						geometryNode.metallicFactor = primitive->material.metallicFactor;
						geometryNode.roughnessFactor = primitive->material.roughnessFactor;
						// @todo: map material id to global texture array
						geometryNodes.push_back(geometryNode);
					}
				}
			}
		}

		for (auto& rangeInfo : buildRangeInfos)
		{
			pBuildRangeInfos.push_back(&rangeInfo);
		}

		// @todo: stage to device

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&geometryNodesBuffer,
			static_cast<uint32_t> (geometryNodes.size() * sizeof(GeometryNode)),
			geometryNodes.data()));

		// Get size info
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = static_cast<uint32_t>(geometries.size());
		accelerationStructureBuildGeometryInfo.pGeometries = geometries.data();

		const uint32_t numTriangles = maxPrimitiveCounts[0];

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(
			device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			maxPrimitiveCounts.data(),
			&accelerationStructureBuildSizesInfo);

		createAccelerationStructureBuffer(bottomLevelAS, accelerationStructureBuildSizesInfo);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = bottomLevelAS.buffer;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &bottomLevelAS.handle);

		// Create a small scratch buffer used during build of the bottom level acceleration structure
		ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationStructureBuildGeometryInfo.dstAccelerationStructure = bottomLevelAS.handle;
		accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		// Build the acceleration structure on the device via a one-time command buffer submission
		// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationStructureBuildGeometryInfo,
			pBuildRangeInfos.data());
		vulkanDevice->flushCommandBuffer(commandBuffer, graphicsQueue);

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = bottomLevelAS.handle;
		bottomLevelAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

		deleteScratchBuffer(scratchBuffer);
	}

	/*
		The top level acceleration structure contains the scene's object instances
	*/
	void createTopLevelAccelerationStructure()
	{
		VkTransformMatrixKHR transformMatrix = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f };

		std::vector<VkAccelerationStructureInstanceKHR> instances;
		VkAccelerationStructureInstanceKHR instance{};
		instance.transform = transformMatrix;
		instance.instanceCustomIndex = 0;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = bottomLevelAS.deviceAddress;
		instances.push_back(instance);

		// Buffer for instance data
		vks::Buffer instancesBuffer;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&instancesBuffer,
			instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
			instances.data()));

		VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
		instanceDataDeviceAddress.deviceAddress = getBufferDeviceAddress(instancesBuffer.buffer);

		VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
		accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

		// Get size info
		/*
		The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored. Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command, except that the hostAddress member of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
		*/
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
			device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			&primitive_count,
			&accelerationStructureBuildSizesInfo);

		createAccelerationStructureBuffer(topLevelAS, accelerationStructureBuildSizesInfo);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = topLevelAS.buffer;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &topLevelAS.handle);

		// Create a small scratch buffer used during build of the top level acceleration structure
		ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationBuildGeometryInfo.dstAccelerationStructure = topLevelAS.handle;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

		// Build the acceleration structure on the device via a one-time command buffer submission
		// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
		vulkanDevice->flushCommandBuffer(commandBuffer, graphicsQueue);

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = topLevelAS.handle;

		deleteScratchBuffer(scratchBuffer);
		instancesBuffer.destroy();
	}
#endif

#if !SPLIT_BLAS
	void createBottomLevelAccelerationStructure3DGRT()
	{
		VkTransformMatrixKHR transformMatrix{};
		auto m = glm::mat3x4(glm::mat4(1.0f));
		memcpy(&transformMatrix, (void*)&m, sizeof(glm::mat3x4));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&transformBuffer3DGRT, sizeof(VkTransformMatrixKHR), &transformMatrix));

		// Build
		VkAccelerationStructureGeometryKHR geometry{};
		VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo;
		VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo;

		uint32_t primitiveCount = gModel.indices.count / 3;
		VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

		vertexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(gModel.vertices.storageBuffer.buffer);
		indexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(gModel.indices.storageBuffer.buffer);
		transformBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(transformBuffer3DGRT.buffer);

		geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		geometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
		geometry.geometry.triangles.maxVertex = gModel.vertices.count;
		geometry.geometry.triangles.vertexStride = sizeof(float) * 3;
		geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
		geometry.geometry.triangles.indexData = indexBufferDeviceAddress;
		geometry.geometry.triangles.transformData = transformBufferDeviceAddress;

		buildRangeInfo.firstVertex = 0;
		buildRangeInfo.primitiveOffset = 0;
		buildRangeInfo.primitiveCount = primitiveCount;
		buildRangeInfo.transformOffset = 0;

		pBuildRangeInfo = &buildRangeInfo;

		// Get size info
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &geometry;

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(
			device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			&primitiveCount,
			&accelerationStructureBuildSizesInfo);

		createAccelerationStructureBuffer(bottomLevelAS3DGRT, accelerationStructureBuildSizesInfo);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = bottomLevelAS3DGRT.buffer;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &bottomLevelAS3DGRT.handle);

		// Create a small scratch buffer used during build of the bottom level acceleration structure
		ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationStructureBuildGeometryInfo.dstAccelerationStructure = bottomLevelAS3DGRT.handle;
		accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		// BLAS Size
		blasSize = accelerationStructureBuildSizesInfo.accelerationStructureSize;

		// Build the acceleration structure on the device via a one-time command buffer submission
		// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		// Timestamp 0: BLAS build start
		vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, ASBuildTimeStampQueryPool, 0);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationStructureBuildGeometryInfo,
			&pBuildRangeInfo);

		// Timestamp 1: BLAS build end
		vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, ASBuildTimeStampQueryPool, 1);
		vulkanDevice->flushCommandBuffer(commandBuffer, graphicsQueue);

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = bottomLevelAS3DGRT.handle;
		bottomLevelAS3DGRT.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

		deleteScratchBuffer(scratchBuffer);
	}

	void createTopLevelAccelerationStructure3DGRT()
	{
		VkTransformMatrixKHR transformMatrix = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f };

		VkAccelerationStructureInstanceKHR instance{};
		instance.transform = transformMatrix;
		instance.instanceCustomIndex = 0;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = bottomLevelAS3DGRT.deviceAddress;

		// Buffer for instance data
		vks::Buffer instancesBuffer;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&instancesBuffer,
			sizeof(VkAccelerationStructureInstanceKHR),
			&instance));

		VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
		instanceDataDeviceAddress.deviceAddress = getBufferDeviceAddress(instancesBuffer.buffer);

		VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
		accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

		// Get size info
		/*
		The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored. Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command, except that the hostAddress member of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
		*/
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

		uint32_t primitive_count = 1;

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(
			device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			&primitive_count,
			&accelerationStructureBuildSizesInfo);

		createAccelerationStructureBuffer(topLevelAS3DGRT, accelerationStructureBuildSizesInfo);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = topLevelAS3DGRT.buffer;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &topLevelAS3DGRT.handle);

		// Create a small scratch buffer used during build of the top level acceleration structure
		ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationBuildGeometryInfo.dstAccelerationStructure = topLevelAS3DGRT.handle;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;
		VkAccelerationStructureBuildRangeInfoKHR* accelerationBuildStructureRangeInfo = &accelerationStructureBuildRangeInfo;

		// TLAS size
		tlasSize = accelerationStructureBuildSizesInfo.accelerationStructureSize;

		// Build the acceleration structure on the device via a one-time command buffer submission
		// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Timestamp 2: TLAS build start
		vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, ASBuildTimeStampQueryPool, 2);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationBuildGeometryInfo,
			&accelerationBuildStructureRangeInfo);

		// Timestamp 3: TLAS build end
		vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, ASBuildTimeStampQueryPool, 3);
		vulkanDevice->flushCommandBuffer(commandBuffer, graphicsQueue);

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = topLevelAS3DGRT.handle;

		deleteScratchBuffer(scratchBuffer);
		instancesBuffer.destroy();
	}
#endif

#if !RAY_QUERY
	/*
		Create the Shader Binding Tables that binds the programs and top-level acceleration structure

		SBT Layout used in VulkanFullRT project:

			/---------------------------------------\
			| Raygen                                |
			|---------------------------------------|
			| Miss(dummy)                           |
			|---------------------------------------|
			| Closest Hit(Basic)                    |
			\---------------------------------------/
	*/
	void createShaderBindingTables() {
		const uint32_t handleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
		const uint32_t handleSizeAligned = vks::tools::alignedSize(rayTracingPipelineProperties.shaderGroupHandleSize, rayTracingPipelineProperties.shaderGroupHandleAlignment);
		const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
		const uint32_t sbtSize = groupCount * handleSizeAligned;

		std::vector<uint8_t> shaderHandleStorage(sbtSize);
		VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

		createShaderBindingTable(shaderBindingTables.raygen, 1);
		createShaderBindingTable(shaderBindingTables.miss, 1);
		createShaderBindingTable(shaderBindingTables.hit, 1);

		// Copy handles
		// Global[0], Group Local[0]: Raygen group
		memcpy(shaderBindingTables.raygen.mapped, shaderHandleStorage.data(), handleSize);
		// Global[1 ~ 2], Group Local[0 ~ 1]: Miss group
		memcpy(shaderBindingTables.miss.mapped, shaderHandleStorage.data() + handleSizeAligned, handleSize);
		// Global[2], Group Local[0]: Hit group
		memcpy(shaderBindingTables.hit.mapped, shaderHandleStorage.data() + handleSizeAligned * 2, handleSize);
	}
#endif

	/*
		Create out gaussianEnclosing pipeline
	*/
	void createGaussianEnclosingPipeline()
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&gaussianEnclosing.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &gaussianEnclosing.pipelineLayout));

		VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(gaussianEnclosing.pipelineLayout, 0);
		computePipelineCreateInfo.stage = loadShader(getShadersPath() + DIR_PATH + "particlePrimitives.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &computePipelineCreateInfo, nullptr, &gaussianEnclosing.pipeline));
	}

#if RAY_QUERY
	void createParticleRenderingPipeline()
	{
		// Specialization constants
		specializationData.windowSizeX = width;
		specializationData.windowSizeY = height;
		std::vector<VkSpecializationMapEntry> specializationMapEntries = {
			vks::initializers::specializationMapEntry(0, 0, sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(1, sizeof(uint32_t), sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(2, sizeof(uint32_t) * 2, sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(3, sizeof(uint32_t) * 3, sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(4, sizeof(uint32_t) * 4, sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(5, sizeof(uint32_t) * 5, sizeof(uint32_t)),
		};
		VkSpecializationInfo specializationInfo = vks::initializers::specializationInfo(static_cast<uint32_t>(specializationMapEntries.size()), specializationMapEntries.data(), sizeof(SpecializationData), &specializationData);

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

		// For transfer push constants
		VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, sizeof(pushConstants), 0);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout, 0);
		computePipelineCreateInfo.stage = loadShader(getShadersPath() + DIR_PATH + "particleRendering.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		computePipelineCreateInfo.stage.pSpecializationInfo = &specializationInfo;

		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &computePipelineCreateInfo, nullptr, &pipeline));
	}
#else
	void createParticleRenderingPipeline()
	{
		// For transfer of num of lights, use specialization constant.
		std::vector<VkSpecializationMapEntry> specializationMapEntries = {
			vks::initializers::specializationMapEntry(0, 0, sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(1, sizeof(uint32_t), sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(2, sizeof(uint32_t) * 2, sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(3, sizeof(uint32_t) * 3, sizeof(uint32_t)),
		};
		VkSpecializationInfo specializationInfo = vks::initializers::specializationInfo(static_cast<uint32_t>(specializationMapEntries.size()), specializationMapEntries.data(), sizeof(SpecializationData), &specializationData);

#if GAUSSIAN_LIGHT_FIELD
		VkDescriptorSetLayout setLayouts[2] = { descriptorSetLayout, gaussianLightField.descriptorSetLayout };
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayouts, 2);
#else
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
#endif
		// For transfer push constants
		VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_RAYGEN_BIT_KHR, sizeof(pushConstants), 0);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
		/*
			Setup ray tracing shader groups
		*/
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

		// Ray generation group
		{
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
			shaderStages[shaderStages.size() - 1].pSpecializationInfo = &specializationInfo;
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			shaderGroups.push_back(shaderGroup);
		}

		// Miss group
		{
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			shaderGroups.push_back(shaderGroup);
#if LOAD_GLTF
			// Second shader for shadows
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroups.push_back(shaderGroup);
#endif
		}

		// Closest hit group 0 : Basic
		{
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "anyhit.rahit.spv", VK_SHADER_STAGE_ANY_HIT_BIT_KHR));
			//shaderStages[shaderStages.size() - 1].pSpecializationInfo = &specializationInfo;
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.anyHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroups.push_back(shaderGroup);
		}

#if GAUSSIAN_LIGHT_FIELD
		// Closest hit group 1 : Gaussian Sampled Sphere
		{
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "sphere.rint.spv", VK_SHADER_STAGE_INTERSECTION_BIT_KHR));
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "sphere.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
			shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size() - 1);
			shaderGroup.intersectionShader = static_cast<uint32_t>(shaderStages.size() - 2);
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroups.push_back(shaderGroup);
		}
#endif

		/*
			Create the ray tracing pipeline
		*/
		VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
		rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		rayTracingPipelineCI.pStages = shaderStages.data();
		rayTracingPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups.size());
		rayTracingPipelineCI.pGroups = shaderGroups.data();
		rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
		rayTracingPipelineCI.layout = pipelineLayout;
		VK_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &pipeline));
	}
#endif

	void createDescriptorSets()
	{
		std::vector<VkDescriptorPoolSize> poolSizes = {
			// ray tracing pipeline
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 * swapChain.imageCount),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 * swapChain.imageCount),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * swapChain.imageCount),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * swapChain.imageCount),
#if SPLIT_BLAS && !RAY_QUERY
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * swapChain.imageCount),
#endif 
#if ENABLE_HIT_COUNTS && !RAY_QUERY
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * swapChain.imageCount),
#endif
#if GAUSSIAN_LIGHT_FIELD
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 * swapChain.imageCount),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, gaussianLightField.samplingCameraNum),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 * swapChain.imageCount), //gaussian sampled sphere
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2), //sampling rays and smpling positions
#endif
		};
#if GAUSSIAN_LIGHT_FIELD
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, swapChain.imageCount + 1); // gaussianEnclosing pipeline + ray tracing pipeline + global descriptor for sampled gaussian light field
#else
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, swapChain.imageCount); // gaussianEnclosing pipeline + ray tracing pipeline
#endif
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));	// descriptor pool
		
		// for ray tracing pipeline begin
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
#if RAY_QUERY
			// Binding 0: Top level acceleration structure
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			// Binding 1: Ray tracing result image
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			// Binding 2: Uniform buffer Dynamic
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
			// Binding 3: Uniform buffer Static
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
			// Binding 4: Storage buffer - Particle Densities
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
			// Binding 5: Storage buffer - Particle Sph Coefficients
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
#else
			// Binding 0: Top level acceleration structure
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0),
			// Binding 1: Ray tracing result image
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1),
			// Binding 2: Uniform buffer Dynamic
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 2),
			// Binding 3: Uniform buffer Static
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 3),
			// Binding 4: Storage buffer - Particle Densities
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 4),
			// Binding 5: Storage buffer - Particle Sph Coefficients
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 5),
	#if SPLIT_BLAS && !RAY_QUERY
			// Binding 6: Storage buffer - primitive Id
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 6),
	#endif
	#if ENABLE_HIT_COUNTS
			// Binding 7: Storage buffer - Ray Hit Count for debugging
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 7),
	#endif
	#if GAUSSIAN_LIGHT_FIELD
			// Binding 8: Sampled Sphere AS
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 8),
			// Binding 9 : Storage buffer - Sampled Sphere
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 9),
	#endif
#endif
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

		for (auto& frame : frameObjects)
		{
			VkDescriptorSet& descriptorSet = frame.descriptorSet;
			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));	// descriptor set

			// WriteDescriptorSet for TLAS (binding0)
			VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = vks::initializers::writeDescriptorSetAccelerationStructureKHR();
			descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
#if SPLIT_BLAS && !RAY_QUERY
			descriptorAccelerationStructureInfo.pAccelerationStructures = &splitBLAS.splittedTLAS.handle;
#else
			descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS3DGRT.handle;
#endif

			VkWriteDescriptorSet accelerationStructureWrite{};
			accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			// The specialized acceleration structure descriptor has to be chained
			accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
			accelerationStructureWrite.dstSet = descriptorSet;
			accelerationStructureWrite.dstBinding = 0;
			accelerationStructureWrite.descriptorCount = 1;
			accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

			VkDescriptorImageInfo storageImageDescriptor = { VK_NULL_HANDLE, swapChain.buffers[frame.imageIndex].view, VK_IMAGE_LAYOUT_GENERAL };

#if GAUSSIAN_LIGHT_FIELD
			VkWriteDescriptorSetAccelerationStructureKHR sampledASInfo = vks::initializers::writeDescriptorSetAccelerationStructureKHR();
			sampledASInfo.accelerationStructureCount = 1;
			sampledASInfo.pAccelerationStructures = &gaussianLightField.gaussianSampledTLAS.handle;

			VkWriteDescriptorSet sampledASWrite{};
			sampledASWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			sampledASWrite.pNext = &sampledASInfo;
			sampledASWrite.dstSet = descriptorSet;
			sampledASWrite.dstBinding = 8;
			sampledASWrite.descriptorCount = 1;
			sampledASWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
#endif

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				// Binding 0: Top level acceleration structure
				accelerationStructureWrite,
				// Binding 1: Ray tracing result image
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor),
				// Binding 2: Uniform data Dynamic
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &frame.uniformBuffer.descriptor),
				// Binding 3: Uniform data Static
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &frame.uniformBufferStatic.descriptor),
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &particleDensities.descriptor),
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &particleSphCoefficients.descriptor),
#if SPLIT_BLAS && !RAY_QUERY
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6, &splitBLAS.d_splittedPrimitiveIdsDeviceAddress.descriptor),
#endif
#if ENABLE_HIT_COUNTS && !RAY_QUERY
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7, &frame.hitCountsbuffer.descriptor),
#endif
#if GAUSSIAN_LIGHT_FIELD
				sampledASWrite,
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 9, &gaussianLightField.gaussianSampledSphereBuffer.descriptor),
#endif
			};

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
		}
		// for ray tracing pipeline end
#if GAUSSIAN_LIGHT_FIELD
		// adding descriptor for sampled data
		std::vector<VkDescriptorSetLayoutBinding> sampledSetLayoutBindings = {
			// Binding 0 : Sampled image
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 0),
			// Binding 1 : Sampled ray dir
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 1),
			// Binding 2 : Sampled camera position
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 2),
		};

		VkDescriptorSetLayoutCreateInfo sampledDescriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(sampledSetLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &sampledDescriptorSetLayoutCI, nullptr, &gaussianLightField.descriptorSetLayout));

		VkDescriptorSetAllocateInfo sampledSetAllocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &gaussianLightField.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &sampledSetAllocInfo, &gaussianLightField.descriptorSet));

		VkDescriptorImageInfo storageImageDescriptor = { VK_NULL_HANDLE, gaussianLightField.imageView, VK_IMAGE_LAYOUT_GENERAL };

		std::vector<VkWriteDescriptorSet> sampledWriteDescriptorSets = {
			vks::initializers::writeDescriptorSet(gaussianLightField.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &storageImageDescriptor),
			vks::initializers::writeDescriptorSet(gaussianLightField.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &gaussianLightField.rayDirBuffer.descriptor),
			vks::initializers::writeDescriptorSet(gaussianLightField.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &gaussianLightField.rayPosBuffer.descriptor), // I need buffer for sampling camera positions
		};

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(sampledWriteDescriptorSets.size()), sampledWriteDescriptorSets.data(), 0, VK_NULL_HANDLE);
#endif
	}

	/*
		Create the descriptor sets used for the gaussianEnclosing pipeline
	*/
	void createGaussianEnclosingDescriptorSets()
	{
		std::vector<VkDescriptorPoolSize> poolSizes = {
			// gaussianEnclosing pipeline
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6),	// vertices, triangles, position, rotation, scale, density
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),	// particle density
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3),	// particle sph coefficient
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1)		// particle totalCount
		};
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1); // gaussianEnclosing pipeline

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));	// descriptor pool

		// for gaussianEnclosing pipeline begin
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0: Vertices
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			// Binding 1: Triangles
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			// Binding 2: Position
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
			// Binding 3: Rotation
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
			// Binding 4: Scale
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
			// Binding 5: Density
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
			// Binding 6: Uniform Buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 6),
			// Binding 7: Particle density
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 7),
			// Binding 8: Features Albedo
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 8),
			// Binding 9: Features Specular
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 9),
			// Binding 10: Particle Sph Coefficient
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 10),
			// Binding 11: Particle Total Counts
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 11),
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &gaussianEnclosing.descriptorSetLayout));

		VkDescriptorSet& descriptorSet = gaussianEnclosing.descriptorSet;
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &gaussianEnclosing.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

		std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
			// Binding 0: Vertices
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &gModel.vertices.storageBuffer.descriptor),
			// Binding 1: Triangles
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &gModel.indices.storageBuffer.descriptor),
			// Binding 2: Position
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &gModel.positions.storageBuffer.descriptor),
			// Binding 3: Rotation
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &gModel.rotations.storageBuffer.descriptor),
			// Binding 4: Scale
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &gModel.scales.storageBuffer.descriptor),
			// Binding 5: Density
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &gModel.densities.storageBuffer.descriptor),
			// Binding 6: Uniform Buffer 
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6, &gaussianEnclosing.uniformBuffer.descriptor),
			// Binding 7: Particle density
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7, &particleDensities.descriptor),
			// Binding 8: Features albedo
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8, &gModel.featuresAlbedo.storageBuffer.descriptor),
			// Binding 9: Features specular
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 9, &gModel.featuresSpecular.storageBuffer.descriptor),
			// Binding 10: Particle sph coefficient
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10, &particleSphCoefficients.descriptor),
			// Binding 11: Particle Total Counts
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 11, &gaussianEnclosing.totalCounts.descriptor),
		};

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, nullptr);
		// for gaussianEnclosing pipeline end
	}

	/*
		If the window has been resized, we need to recreate the storage image and it's descriptor
	*/
	void handleResize()
	{
		VK_CHECK_RESULT(vkDeviceWaitIdle(device));
		for (FrameObject& frame : frameObjects) {

			VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, swapChain.buffers[frame.imageIndex].view, VK_IMAGE_LAYOUT_GENERAL };
			VkWriteDescriptorSet resultImageWrite = vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor);
			vkUpdateDescriptorSets(device, 1, &resultImageWrite, 0, VK_NULL_HANDLE);
		}
		resized = false;
	}

	void buildGaussianEnclosingCommandBuffer()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VK_CHECK_RESULT(vkBeginCommandBuffer(gaussianEnclosing.commandBuffer, &cmdBufInfo));

		vkCmdBindPipeline(gaussianEnclosing.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gaussianEnclosing.pipeline);
		vkCmdBindDescriptorSets(gaussianEnclosing.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gaussianEnclosing.pipelineLayout, 0, 1, &gaussianEnclosing.descriptorSet, 0, 0);

		uint32_t groupCountX = NUM_OF_GAUSSIANS;
		vkCmdDispatch(gaussianEnclosing.commandBuffer, (gModel.splatSet.size() + groupCountX - 1)/ groupCountX, 1, 1);

		VK_CHECK_RESULT(vkEndCommandBuffer(gaussianEnclosing.commandBuffer));
	}

	/*
		Command buffer record
	*/
	void buildCommandBuffer(FrameObject& frame)
	{
		if (resized)
		{
			handleResize();
		}
		vkResetCommandBuffer(frame.commandBuffer, 0);
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		VK_CHECK_RESULT(vkBeginCommandBuffer(frame.commandBuffer, &cmdBufInfo));

		vkCmdResetQueryPool(frame.commandBuffer, frame.timeStampQueryPool, 0, static_cast<uint32_t>(frame.timeStamps.size()));

#if RAY_QUERY
		vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
		vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &frame.descriptorSet, 0, 0);
		vkCmdPushConstants(frame.commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);
#else
		vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
		vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &frame.descriptorSet, 0, 0);
#if GAUSSIAN_LIGHT_FIELD
		vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 1, 1, &gaussianLightField.descriptorSet, 0, 0);
#endif
		vkCmdPushConstants(frame.commandBuffer, pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(pushConstants), &pushConstants);
#endif

		vks::tools::setImageLayout(
			frame.commandBuffer,
			swapChain.images[frame.imageIndex],
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			subresourceRange);

#if RAY_QUERY
		vkCmdDispatch(frame.commandBuffer, (width + TB_SIZE_X - 1) / TB_SIZE_X, (height + TB_SIZE_Y - 1) / TB_SIZE_Y, 1);
#else
		VkStridedDeviceAddressRegionKHR emptySbtEntry = {};
		vkCmdTraceRaysKHR(
			frame.commandBuffer,
			&shaderBindingTables.raygen.stridedDeviceAddressRegion,
			&shaderBindingTables.miss.stridedDeviceAddressRegion,
			&shaderBindingTables.hit.stridedDeviceAddressRegion,
			&emptySbtEntry,
			width,
			height,
			1);
#endif

		vks::tools::setImageLayout(
			frame.commandBuffer,
			swapChain.images[frame.imageIndex],
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			subresourceRange);

		drawUI(frame.commandBuffer, frameBuffers[frame.imageIndex], frame.vertexBuffer, frame.indexBuffer);

		vkCmdWriteTimestamp(frame.commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.timeStampQueryPool, 0);

		VK_CHECK_RESULT(vkEndCommandBuffer(frame.commandBuffer));
	}

	virtual void buildCommandBuffers()
	{
		if (resized)
		{
			handleResize();
		}

		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		for (auto& frame : frameObjects)
		{
			VK_CHECK_RESULT(vkBeginCommandBuffer(frame.commandBuffer, &cmdBufInfo));

			vkCmdResetQueryPool(frame.commandBuffer, frame.timeStampQueryPool, 0, static_cast<uint32_t>(frame.timeStamps.size()));

#if RAY_QUERY
			vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
			vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &frame.descriptorSet, 0, 0);
			vkCmdPushConstants(frame.commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);
#else
			vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
			vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &frame.descriptorSet, 0, 0);
#if GAUSSIAN_LIGHT_FIELD
			vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 1, 1, &gaussianLightField.descriptorSet, 0, 0);
#endif
			vkCmdPushConstants(frame.commandBuffer, pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(pushConstants), &pushConstants);
#endif

			vks::tools::setImageLayout(
				frame.commandBuffer,
				swapChain.images[frame.imageIndex],
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				subresourceRange);

#if RAY_QUERY
			vkCmdDispatch(frame.commandBuffer, width, height, 1);
#else
			VkStridedDeviceAddressRegionKHR emptySbtEntry = {};
			vkCmdTraceRaysKHR(
				frame.commandBuffer,
				&shaderBindingTables.raygen.stridedDeviceAddressRegion,
				&shaderBindingTables.miss.stridedDeviceAddressRegion,
				&shaderBindingTables.hit.stridedDeviceAddressRegion,
				&emptySbtEntry,
				width,
				height,
				1);
#endif

			vks::tools::setImageLayout(
				frame.commandBuffer,
				swapChain.images[frame.imageIndex],
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				subresourceRange);

			drawUI(frame.commandBuffer, frameBuffers[frame.imageIndex]);

			vkCmdWriteTimestamp(frame.commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.timeStampQueryPool, 0);

			VK_CHECK_RESULT(vkEndCommandBuffer(frame.commandBuffer));
		}
	}

#if DYNAMIC_CAMERA
	glm::mat4 getRotatingCameraPose(float radius = 3.0f)
	{
		float theta = glm::radians(timer * 360.0f);
		glm::vec3 camPos = glm::vec3(radius * cos(theta), radius * 2 * sin(theta), radius);
		glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 up = glm::vec3(0.0f, 0.0f, -1.0f);

		glm::vec3 forward = glm::normalize(target - camPos);
		glm::vec3 right = glm::normalize(glm::cross(up, forward));
		glm::vec3 true_up = glm::cross(forward, right);

		glm::mat4 C2W(1.0f);
		C2W[0] = glm::vec4(right, 0.0f);
		C2W[1] = -glm::vec4(true_up, 0.0f);	// opposite from NeRF camera
		C2W[2] = -glm::vec4(forward, 0.0f);	// opposite from NeRF camera (since forward is -N in Vulkan)
		C2W[3] = glm::vec4(camPos, 1.0f);
		return C2W;
	}
#endif

	void updateUniformBuffer()
	{
#if DYNAMIC_CAMERA
		//uniformDataDynamic.viewInverse = glm::inverse(getRotatingCameraPose());
		uniformDataDynamic.viewInverse = getRotatingCameraPose();
		uniformDataDynamic.projInverse = glm::inverse(camera.matrices.perspective);
#else
#if QUATERNION_CAMERA
		uniformDataDynamic.viewInverse = glm::inverse(quaternionCamera.getViewMatrix());
		uniformDataDynamic.projInverse = glm::inverse(quaternionCamera.perspective);
#else
		uniformDataDynamic.viewInverse = glm::inverse(camera.matrices.view);
		uniformDataDynamic.projInverse = glm::inverse(camera.matrices.perspective);
#endif
#endif

		FrameObject currentFrame = frameObjects[getCurrentFrameIndex()];
		memcpy(currentFrame.uniformBuffer.mapped, &uniformDataDynamic, sizeof(uniformDataDynamic));
	}

	void updateGaussianEnclosingUniformBuffer()
	{
		gaussianEnclosingUniformData.numOfGaussians = gModel.splatSet.size();
		gaussianEnclosingUniformData.kernelMinResponse = 0.0113f;	// these values should be managed as config val
		gaussianEnclosingUniformData.opts = vks::utils::MOGRenderNone;
		gaussianEnclosingUniformData.degree = 4;

		// mapping
		vks::Buffer stagingBuffer;
		stagingBuffer.size = sizeof(vks::utils::GaussianEnclosingUniformData);
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.size, &stagingBuffer.buffer, &stagingBuffer.memory, nullptr));

		void* data;
		vkMapMemory(vulkanDevice->logicalDevice, stagingBuffer.memory, 0, sizeof(vks::utils::GaussianEnclosingUniformData), 0, &data);
		memcpy(data, (void*)&gaussianEnclosingUniformData, sizeof(vks::utils::GaussianEnclosingUniformData));
		vkUnmapMemory(vulkanDevice->logicalDevice, stagingBuffer.memory);

		VkBufferCopy copyRegion;
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = stagingBuffer.size;
		vulkanDevice->copyBuffer(&stagingBuffer, &gaussianEnclosing.uniformBuffer, graphicsQueue, &copyRegion);

		vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer.buffer, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, stagingBuffer.memory, nullptr);
	}

	virtual void getEnabledFeatures()
	{
		// New Features using VkPhysicalDeviceFeatures2 structure.
#if RAY_QUERY
		// Enable feature required for ray query.
		enabledRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
		enabledRayQueryFeatures.rayQuery = VK_TRUE;
		
		physicalDeviceHostQueryResetFeatures.pNext = &enabledRayQueryFeatures;
#endif

		// Enable feature required for time stamp command pool reset.
		physicalDeviceHostQueryResetFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT;
		physicalDeviceHostQueryResetFeatures.hostQueryReset = VK_TRUE;

		// Enable features required for ray tracing using feature chaining via pNext		
		enabledBufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
		enabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;
		enabledBufferDeviceAddresFeatures.pNext = &physicalDeviceHostQueryResetFeatures;

		enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
		enabledRayTracingPipelineFeatures.pNext = &enabledBufferDeviceAddresFeatures;

		enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
		enabledAccelerationStructureFeatures.pNext = &enabledRayTracingPipelineFeatures;

		physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
		physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
		physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
		physicalDeviceDescriptorIndexingFeatures.pNext = &enabledAccelerationStructureFeatures;

		deviceCreatepNextChain = &physicalDeviceDescriptorIndexingFeatures;

		enabledFeatures.shaderInt64 = VK_TRUE;	// Buffer device address requires the 64-bit integer feature to be enabled
		enabledFeatures.samplerAnisotropy = VK_TRUE;
	}

	virtual void getEnabledExtensions()
	{
		enableExtensions();

		enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::PreTransformVertices;
		vkglTF::bufferUsageFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

#if LOAD_GLTF
		scene.loadFromFile(getAssetPath() + ASSET_PATH, vulkanDevice, graphicsQueue, glTFLoadingFlags);

		// Cubemap texture
		loadCubemap(getAssetPath() + CUBEMAP_TEXTURE_PATH, VK_FORMAT_R8G8B8A8_UNORM);
#endif

		//gModel.load3DGRTObject(getAssetPath() + "3DGRTModels/lego/ckpt_last.pt", vulkanDevice);
		gModel.load3DGRTModel(getAssetPath() + ASSET_PATH + PLY_FILE, vulkanDevice);
	}

	bool initVulkan() {
		// Auto-compile shaders
		// Remove 'pause' from batch file for speedy execution
		std::cout << "*** Shader compilation BEGIN ***\n";
		system("cd ..\\shaders\\glsl\\base\\ && baseCompile.bat");
		std::cout << "\t...base project shaders compile completed.\n";
		system("cd ..\\shaders\\glsl\\VulkanFullRT\\ && VulkanFullRTCompile.bat");
		std::cout << "\t...Vulkan FullRT project shaders compile completed.\n";
		std::cout << "*** Shader compilation END ***\n";

		bool result = VulkanRTBase::initVulkan();
		if (!result) {
			std::cout << "VulkanFullRT initVulkan failed.\n";
			return false;
		}

		return true;
	}

#if !SPLIT_BLAS
	void initASBuildTimestamp()
	{
		ASBuildTimeStamps.resize(4);

		VkQueryPoolCreateInfo queryPoolInfo{};
		queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryPoolInfo.queryCount = static_cast<uint32_t>(ASBuildTimeStamps.size());
		VK_CHECK_RESULT(vkCreateQueryPool(device, &queryPoolInfo, nullptr, &ASBuildTimeStampQueryPool));

		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdResetQueryPool(commandBuffer, ASBuildTimeStampQueryPool, 0, static_cast<uint32_t>(ASBuildTimeStamps.size()));
		vulkanDevice->flushCommandBuffer(commandBuffer, graphicsQueue);
	}

	void printASBuildInfo()
	{
		vkGetQueryPoolResults(device, ASBuildTimeStampQueryPool, 0, ASBuildTimeStamps.size(), ASBuildTimeStamps.size() * sizeof(uint64_t), ASBuildTimeStamps.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

		VkPhysicalDeviceLimits device_limits = deviceProperties.limits;
		float delta_in_ms_BLAS = float(ASBuildTimeStamps[1] - ASBuildTimeStamps[0]) * device_limits.timestampPeriod / 1000000.0f;
		float delta_in_ms_TLAS = float(ASBuildTimeStamps[3] - ASBuildTimeStamps[2]) * device_limits.timestampPeriod / 1000000.0f;
#if defined(_WIN32)
		std::cout << "\n*** AS Build Info BEGIN ***\n";
		std::cout << "BLAS build time: " << delta_in_ms_BLAS << " (ms)\n";
		std::cout << "TLAS build time: " << delta_in_ms_TLAS << " (ms)\n";
		std::cout << "BLAS size: " << blasSize << " (Bytes)\n";
		std::cout << "TLAS size: " << tlasSize << " (Bytes)\n";
		std::cout << "*** AS Build Info END ***\n";
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
		LOGD("\n*** AS Build Info BEGIN ***\n");
		LOGD("BLAS build time: %f (ms))\n", delta_in_ms_BLAS);
		LOGD("TLAS build time: %f (ms))\n", delta_in_ms_TLAS);
		LOGD("BLAS size: %llu (Bytes))\n", blasSize);
		LOGD("TLAS size: %llu (Bytes))\n", tlasSize);
		LOGD("*** AS Build Info END ***\n");
#endif
	}
#endif

	void computeGaussianEnclosingIcosaHedron()
	{
		// gaussianEnclosing pipeline
		buildGaussianEnclosingCommandBuffer();

		VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &gaussianEnclosing.commandBuffer;
		VK_CHECK_RESULT(vkQueueSubmit(graphicsQueue, 1, &computeSubmitInfo, VK_NULL_HANDLE));

		vkDeviceWaitIdle(device);
	}

#if GAUSSIAN_LIGHT_FIELD
	//light field add
	vector<glm::vec3> vertices = {
		glm::vec3(1, 0, 0),
		glm::vec3(-1, 0, 0),
		glm::vec3(0, 1, 0),
		glm::vec3(0, -1, 0),
		glm::vec3(0, 0, 1),
		glm::vec3(0, 0, -1)
	};

	int addVertex(const glm::vec3& v) {
		auto it = vertexMap.find(v);
		if (it != vertexMap.end()) {
			return it->second; // 이미 있으면 기존 인덱스 반환
		}
		int idx = (int)vertices.size();
		vertices.push_back(v);   // 새 정점 추가
		vertexMap[v] = idx;      // 정점 v → 인덱스 idx 매핑
		return idx;
	}

	void calculateGaussianLightFieldSamples() {
		//VkPhysicalDeviceProperties props{};
		//vkGetPhysicalDeviceProperties(physicalDevice, &props);

		//// work group 관련 최대값
		//std::cout << "maxComputeWorkGroupCount : "
		//	<< props.limits.maxComputeWorkGroupCount[0] << ", "
		//	<< props.limits.maxComputeWorkGroupCount[1] << ", "
		//	<< props.limits.maxComputeWorkGroupCount[2] << std::endl;

		//std::cout << "maxComputeWorkGroupSize : "
		//	<< props.limits.maxComputeWorkGroupSize[0] << ", "
		//	<< props.limits.maxComputeWorkGroupSize[1] << ", "
		//	<< props.limits.maxComputeWorkGroupSize[2] << std::endl;

		//std::cout << "maxComputeWorkGroupInvocations : "
		//	<< props.limits.maxComputeWorkGroupInvocations << std::endl;

		//calcualte gaussian light field sample points and directions of each points
		const unsigned int level = gaussianLightField.sphereRedivisionLevel;
		gaussianLightField.uniformDataStatic.level = level;
		
		float minX, minY, minZ, maxX, maxY, maxZ;
		minX = minY = minZ = FLT_MAX;
		maxX = maxY = maxZ = FLT_MIN;
		
		//calcuate max/min positions of gaussians
		int iterMax = gModel.splatSet.size();
		for (int i = 0; i < iterMax; i++) {
			float x = gModel.splatSet.positions[i * 3];
			float y = gModel.splatSet.positions[i * 3 + 1];
			float z = gModel.splatSet.positions[i * 3 + 2];

			if (x < minX) minX = x;
			if (y < minY) minY = y;
			if (z < minZ) minZ = z;
			if (x > maxX) maxX = x;
			if (y > maxY) maxY = y;
			if (z > maxZ) maxZ = z;
		}

		//sample space is defined by sphere
		gaussianLightField.gaussianSampledSphere.center = glm::vec3((minX + maxX) / 2, (minY + maxY) / 2, (minZ + maxZ) / 2);
		const glm::vec3 center = gaussianLightField.gaussianSampledSphere.center;

		float candX = maxX - minX; float candY = maxY - minY; float candZ = maxZ - minZ;
		const float radius = gaussianLightField.gaussianSampledSphere.radius = max(max(candX, candY), candZ) / 2;
		gaussianLightField.uniformDataStatic.sphereInfo = glm::vec4(center, radius);
		glm::vec3 up = glm::vec3(0, 0, 1);

		glm::vec3 min = gaussianLightField.gaussianSampledSphere.center - gaussianLightField.gaussianSampledSphere.radius;
		glm::vec3 max = gaussianLightField.gaussianSampledSphere.center + gaussianLightField.gaussianSampledSphere.radius;

		gaussianLightField.gaussianSampledSphereAABB.minX = min.x;
		gaussianLightField.gaussianSampledSphereAABB.minY = min.y;
		gaussianLightField.gaussianSampledSphereAABB.minZ = min.z;
		gaussianLightField.gaussianSampledSphereAABB.maxX = max.x;
		gaussianLightField.gaussianSampledSphereAABB.maxY = max.y;
		gaussianLightField.gaussianSampledSphereAABB.maxZ = max.z;

		const float offset = gaussianLightField.gaussianSampledSphere.radius / 16;

		for (int i = 0; i < 6; i++) {
			auto& vertex = vertices[i];
			vertexMap[vertex] = i;
		}

		vector<TriangleIdx> faces = {
			//these indexes are (z, x, y) order
			TriangleIdx(4, 0, 2, "A0"),
			TriangleIdx(4, 2, 1, "A1"),
			TriangleIdx(4, 1, 3, "A2"),
			TriangleIdx(4, 3, 0, "A3"),
			TriangleIdx(5, 0, 2, "B0"),
			TriangleIdx(5, 2, 1, "B1"),
			TriangleIdx(5, 1, 3, "B2"),
			TriangleIdx(5, 3, 0, "B3"),
		};

		for (int i = 0; i < level; i++) {
			//vertices of devided sphere would be used for ray origin positions
			//iteration of this loop is to define how many times to devide the sphere. 3 = 512, 4 = 2048, 5 = 8192 meshes.
			//I recommend to set iterations as 4, cause vulkan's image array layer is max to 2048.
			vector<TriangleIdx> new_faces;
			for (auto& face : faces) {
				glm::vec3 v1 = vertices[face.v1];
				glm::vec3 v2 = vertices[face.v2];
				glm::vec3 v3 = vertices[face.v3];

				glm::vec3 m1 = glm::normalize(v2 + v3);
				glm::vec3 m2 = glm::normalize(v3 + v1);
				glm::vec3 m3 = glm::normalize(v1 + v2);

				//detecting redundancy of vertices
				int i1 = addVertex(m1);
				int i2 = addVertex(m2);
				int i3 = addVertex(m3);

				string pLabel = face.label;

				new_faces.push_back(TriangleIdx(i1, i2, i3, pLabel + "0"));
				new_faces.push_back(TriangleIdx(face.v1, i3, i2, pLabel + "1"));
				new_faces.push_back(TriangleIdx(i3, face.v2, i1, pLabel + "2"));
				new_faces.push_back(TriangleIdx(i2, i1, face.v3, pLabel + "3"));
			}
			faces = new_faces;
		}

		gaussianLightField.samplingCameraNum = (vertices.size() + gaussianLightField.uniformDataStatic.width * gaussianLightField.uniformDataStatic.height - 1) / (gaussianLightField.uniformDataStatic.width * gaussianLightField.uniformDataStatic.height);
		gaussianLightField.sampleImageWidth = pow(2, level + 1) * gaussianLightField.uniformDataStatic.width;
		gaussianLightField.sampleImageHeight = pow(2, level + 2) * gaussianLightField.uniformDataStatic.height;
		
		vector<glm::vec2>& rayPos = gaussianLightField.rayPos;
		vector<glm::vec2>& rayDir = gaussianLightField.rayDir;
		
		for (auto& v : vertices) {
			float phi = atan2(v.z, sqrt(v.x * v.x + v.y * v.y));
			float theta = atan2(v.y, v.x);
			
			rayPos.push_back(glm::vec2(theta, phi));
		}
		
		//barycentric position will be used for ray directions
		for (auto& face : faces) {
			glm::vec3 barycenter = (vertices[face.v1] + vertices[face.v2] + vertices[face.v3]) / glm::vec3(3.0f, 3.0f, 3.0f);
			barycenter = glm::normalize(barycenter);
			float phi = atan2(barycenter.z, sqrt(barycenter.x * barycenter.x + barycenter.y * barycenter.y));
			float theta = atan2(barycenter.y, barycenter.x);
			
			rayDir.push_back(glm::vec2(theta, phi));
		}
		cout << gaussianLightField.samplingCameraNum << endl;
		//cout << gaussianLightField.samplingCameraNum * gaussianLightField.sampleImageHeight * gaussianLightField.sampleImageWidth << " " << rayPos.size() * rayDir.size() << endl;
		//cout << rayPos.size() / (gaussianLightField.uniformDataStatic.width * gaussianLightField.uniformDataStatic.height) << endl;
		cout << "current calculation needs : " << rayPos.size() * rayDir.size() << endl;
		//vec4 rayDirection = vec4(cos(phi_d) * cos(theta_d), cos(phi_d) * sin(theta_d), sin(phi_d), 0.0f);

		// Need to adjust alignment
		// add giving data to rayPosBuffer
		
		//method using Camera class in camera.hpp
		/*Camera camera;
		camera.setPosition(cameraPos);
		camera.setRotation(glm::vec3(0, 0, 0));
		camera.setPerspective(90.0f, 1, NEAR_PLANE, FAR_PLANE);
		camera.updateAspectRatio(1.0f);*/

	}

	void createGaussianLightFieldImages() {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = gaussianLightField.sampleImageWidth;
		imageInfo.extent.height = gaussianLightField.sampleImageHeight;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = gaussianLightField.samplingCameraNum;
		imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK_RESULT(vkCreateImage(device, &imageInfo, nullptr, &gaussianLightField.image));
		
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, gaussianLightField.image, &memReqs);
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReqs.size;
		//cout << memReqs.size << " " << gaussianLightField.samplingCameraNum * gaussianLightField.sampleImageHeight * gaussianLightField.sampleImageWidth * 4 << endl;
		allocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//below crashes from level 7.
		VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &gaussianLightField.imageMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, gaussianLightField.image, gaussianLightField.imageMemory, 0));

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = gaussianLightField.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		viewInfo.format = imageInfo.format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = imageInfo.arrayLayers;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &gaussianLightField.imageView));
	}

	void createGaussianLightFieldDescriptorSets() {
		std::vector<VkDescriptorPoolSize> poolSizes = {
			// ray tracing pipeline
			// binding 0 : TLAS for gaussian structure
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1),
			// binding 1 : image
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1),
			// binding 2 : static info
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			// binding 3, 4 : rayDirs, rayPos
			// binding 5, 6 : buffer references(ParticleDensities, ParticleSphCoefficients)
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4),
#if SPLIT_BLAS && !RAY_QUERY
			// binding 7 : primitives id
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),
#endif 
		};
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));	// descriptor pool

		// for ray tracing pipeline begin
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
#if RAY_QUERY
			// Binding 0: Top level acceleration structure
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			// Binding 1: Ray tracing result image
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			// Binding 2: Uniform buffer Static
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
			// Binding 3: Camera Information - viewInverseMatrix
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
			// Binding 4 : rayDirs
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
			// Binding 5: Storage buffer - Particle Densities
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
			// Binding 6: Storage buffer - Particle Sph Coefficients
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 6),
#else
			// Binding 0: Top level acceleration structure
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0),
			// Binding 1: Ray tracing result image
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1),
			// Binding 2: Uniform buffer Static
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 2),
			// Binding 3 : rayDirs
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 3),
			// Binding 4 : rayPos
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 4),
			// Binding 5: Storage buffer - Particle Densities
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 5),
			// Binding 6: Storage buffer - Particle Sph Coefficients
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 6),
	#if SPLIT_BLAS && !RAY_QUERY
			// Binding 8: Storage buffer - primitive Id
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 7),
	#endif
	//#if ENABLE_HIT_COUNTS
	//		// Binding 7: Storage buffer - Ray Hit Count for debugging
	//		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 7),
	//#endif
#endif
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &gaussianLightField.descriptorSetLayout));

		{
			VkDescriptorSet& descriptorSet = gaussianLightField.descriptorSet;
			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &gaussianLightField.descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &gaussianLightField.descriptorSet));	// descriptor set

			// WriteDescriptorSet for TLAS (binding0)
			VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = vks::initializers::writeDescriptorSetAccelerationStructureKHR();
			descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
#if SPLIT_BLAS && !RAY_QUERY
			descriptorAccelerationStructureInfo.pAccelerationStructures = &splitBLAS.splittedTLAS.handle;
#else
			descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS3DGRT.handle;
#endif

			VkWriteDescriptorSet accelerationStructureWrite{};
			accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			// The specialized acceleration structure descriptor has to be chained
			accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
			accelerationStructureWrite.dstSet = descriptorSet;
			accelerationStructureWrite.dstBinding = 0;
			accelerationStructureWrite.descriptorCount = 1;
			accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

			VkDescriptorImageInfo storageImageDescriptor = { VK_NULL_HANDLE, gaussianLightField.imageView, VK_IMAGE_LAYOUT_GENERAL };

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				// Binding 0: Top level acceleration structure
				accelerationStructureWrite,
				// Binding 1: Ray tracing result image
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor),
				// Binding 2: Uniform data Static
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &gaussianLightField.uniformBufferStatic.descriptor),
				// Binding 3: rayDir
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &gaussianLightField.rayDirBuffer.descriptor),
				// Binding 4: rayPos
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &gaussianLightField.rayPosBuffer.descriptor),
				// Binding 5: particle densities
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &particleDensities.descriptor),
				// Binding 6: particle Sph Coefficients
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6, &particleSphCoefficients.descriptor),
#if SPLIT_BLAS && !RAY_QUERY
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7, &splitBLAS.d_splittedPrimitiveIdsDeviceAddress.descriptor),
#endif
//#if ENABLE_HIT_COUNTS && !RAY_QUERY
//				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7, &frame.hitCountsbuffer.descriptor),
//#endif
			};

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);

		}
		// for ray tracing pipeline end
	}

	void createGaussianLightFieldPipeline() {
		// For transfer of num of lights, use specialization constant. But not using in this situation
		std::vector<VkSpecializationMapEntry> specializationMapEntries = {
			vks::initializers::specializationMapEntry(0, 0, sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(1, sizeof(uint32_t), sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(2, sizeof(uint32_t) * 2, sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(3, sizeof(uint32_t) * 3, sizeof(uint32_t)),
		};
		VkSpecializationInfo specializationInfo = vks::initializers::specializationInfo(static_cast<uint32_t>(specializationMapEntries.size()), specializationMapEntries.data(), sizeof(SpecializationData), &specializationData);

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&gaussianLightField.descriptorSetLayout, 1);

		// For transfer push constants
		VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_RAYGEN_BIT_KHR, sizeof(pushConstants), 0);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &gaussianLightField.pipelineLayout));
		/*
			Setup ray tracing shader groups
		*/
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

		// Ray generation group
		{
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "raygenGaussianLightField.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
			shaderStages[shaderStages.size() - 1].pSpecializationInfo = &specializationInfo;
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			gaussianLightField.shaderGroups.push_back(shaderGroup);
		}

		// Miss group
		{
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			gaussianLightField.shaderGroups.push_back(shaderGroup);
#if LOAD_GLTF
			// Second shader for shadows
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			gaussianLightField.shaderGroups.push_back(shaderGroup);
#endif
		}

		// Closest hit group 0 : Basic
		{
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "anyhit.rahit.spv", VK_SHADER_STAGE_ANY_HIT_BIT_KHR));
			//shaderStages[shaderStages.size() - 1].pSpecializationInfo = &specializationInfo;
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.anyHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			gaussianLightField.shaderGroups.push_back(shaderGroup);
		}
		/*
			Create the ray tracing pipeline
		*/
		VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
		rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		rayTracingPipelineCI.pStages = shaderStages.data();
		rayTracingPipelineCI.groupCount = static_cast<uint32_t>(gaussianLightField.shaderGroups.size());
		rayTracingPipelineCI.pGroups = gaussianLightField.shaderGroups.data();
		rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
		rayTracingPipelineCI.layout = gaussianLightField.pipelineLayout;
		
		VK_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &gaussianLightField.pipeline));
	}

	void createGuassianLightFieldShaderBindingTables() {
		const uint32_t handleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
		const uint32_t handleSizeAligned = vks::tools::alignedSize(rayTracingPipelineProperties.shaderGroupHandleSize, rayTracingPipelineProperties.shaderGroupHandleAlignment);
		const uint32_t groupCount = static_cast<uint32_t>(gaussianLightField.shaderGroups.size());
		const uint32_t sbtSize = groupCount * handleSizeAligned;

		std::vector<uint8_t> shaderHandleStorage(sbtSize);
		VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesKHR(device, gaussianLightField.pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

		createShaderBindingTable(gaussianLightField.shaderBindingTables.raygen, 1);
		createShaderBindingTable(gaussianLightField.shaderBindingTables.miss, 1);
		createShaderBindingTable(gaussianLightField.shaderBindingTables.hit, 1);

		// Copy handles
		// Global[0], Group Local[0]: Raygen group
		memcpy(gaussianLightField.shaderBindingTables.raygen.mapped, shaderHandleStorage.data(), handleSize);
		// Global[1 ~ 2], Group Local[0 ~ 1]: Miss group
		memcpy(gaussianLightField.shaderBindingTables.miss.mapped, shaderHandleStorage.data() + handleSizeAligned, handleSize);
		// Global[2], Group Local[0]: Hit group
		memcpy(gaussianLightField.shaderBindingTables.hit.mapped, shaderHandleStorage.data() + handleSizeAligned * 2, handleSize);
	}

	void computeGaussianLightField() {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		VK_CHECK_RESULT(vkBeginCommandBuffer(gaussianLightField.commandBuffer, &beginInfo));

		VkImageMemoryBarrier imageBarrier{};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageBarrier.srcAccessMask = 0;
		imageBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imageBarrier.image = gaussianLightField.image;
		imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier.subresourceRange.baseMipLevel = 0;
		imageBarrier.subresourceRange.levelCount = 1;
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.layerCount = gaussianLightField.samplingCameraNum;

		vkCmdPipelineBarrier(gaussianLightField.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

		vkCmdBindPipeline(gaussianLightField.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, gaussianLightField.pipeline);

		vkCmdBindDescriptorSets(gaussianLightField.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, gaussianLightField.pipelineLayout,	0, 1, &gaussianLightField.descriptorSet, 0, nullptr);
		
		VkStridedDeviceAddressRegionKHR emptySbtEntry = {};
		vkCmdTraceRaysKHR(gaussianLightField.commandBuffer, &gaussianLightField.shaderBindingTables.raygen.stridedDeviceAddressRegion, &gaussianLightField.shaderBindingTables.miss.stridedDeviceAddressRegion, &gaussianLightField.shaderBindingTables.hit.stridedDeviceAddressRegion, &emptySbtEntry, gaussianLightField.rayPos.size(), gaussianLightField.rayDir.size(), 1);
		
		//VkImageMemoryBarrier postBarrier{};
		//postBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		//postBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		//postBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		//postBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		//postBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		//postBarrier.image = gaussianLightField.image;
		//postBarrier.subresourceRange = imageBarrier.subresourceRange;

		//vkCmdPipelineBarrier(gaussianLightField.commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &postBarrier);

		VK_CHECK_RESULT(vkEndCommandBuffer(gaussianLightField.commandBuffer));

		VkSubmitInfo submitInfo = vks::initializers::submitInfo();
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &gaussianLightField.commandBuffer;
		
		VK_CHECK_RESULT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
		VK_CHECK_RESULT(vkDeviceWaitIdle(device));

		//for check sampling direction and position is correct or not
#if IMAGE_CHECK
		vks::Buffer stagingBuffer;
		VkDeviceSize totalImageSize = gaussianLightField.samplingCameraNum * gaussianLightField.sampleImageWidth * gaussianLightField.sampleImageHeight * 4;
		
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, totalImageSize, nullptr));
		vulkanDevice->copyImagesToBuffer(gaussianLightField.image, stagingBuffer, graphicsQueue, VK_IMAGE_LAYOUT_GENERAL, gaussianLightField.sampleImageWidth, gaussianLightField.sampleImageHeight, gaussianLightField.samplingCameraNum);
		VK_CHECK_RESULT(vkMapMemory(device, stagingBuffer.memory, 0, stagingBuffer.size, 0, &stagingBuffer.mapped));

		int unsuccess = 0;
		const unsigned int imageSize = gaussianLightField.sampleImageHeight * gaussianLightField.sampleImageWidth * 4;
		for (unsigned int i = 0; i < gaussianLightField.samplingCameraNum; i++) {
			if (i < gaussianLightField.samplingCameraNum - 2) continue;
			std::ostringstream oss;
			oss << "sampling_cam" << std::setw(4) << std::setfill('0') << i << ".png";
			std::string fileName = oss.str();
			uint8_t* imageStart = (uint8_t*)stagingBuffer.mapped + i * imageSize;
			
			int success = stbi_write_png(fileName.c_str(), gaussianLightField.sampleImageWidth, gaussianLightField.sampleImageHeight, 4, imageStart, gaussianLightField.sampleImageWidth * 4);
		}

		if (!unsuccess) {
			std::cout << "successfully write sampling data to .png file.\n";
		}
		else {
			std::cout << "failed to write sampling data to .png file.\n";
		}

		vkUnmapMemory(device, stagingBuffer.memory);
		vkFreeMemory(device, stagingBuffer.memory, nullptr);
#endif
	}

	void cleanupGaussianLightFieldComponents() {
		vkDestroyPipeline(device, gaussianLightField.pipeline, nullptr);
		vkDestroyPipelineLayout(device, gaussianLightField.pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, gaussianLightField.descriptorSetLayout, nullptr);

		gaussianLightField.uniformBufferStatic.destroy();		
	}

	void createGaussianLightFieldBLAS() {
		uint32_t primitiveCount = 1;

		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&gaussianLightField.aabbBuffer, sizeof(gaussianLightField.gaussianSampledSphereAABB), &gaussianLightField.gaussianSampledSphereAABB));

		// Build
		VkAccelerationStructureGeometryKHR geometry{};
		VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
		VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo;

		VkDeviceOrHostAddressConstKHR aabbBufferDeviceAddress{};
		aabbBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(gaussianLightField.aabbBuffer.buffer);

		geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
		geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
		geometry.geometry.aabbs.data = aabbBufferDeviceAddress;
		geometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);

		buildRangeInfo.primitiveCount = primitiveCount;

		pBuildRangeInfo = &buildRangeInfo;

		// Get size info
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &geometry;

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(
			device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			&primitiveCount,
			&accelerationStructureBuildSizesInfo);

		createAccelerationStructureBuffer(gaussianLightField.gaussianSampledBLAS, accelerationStructureBuildSizesInfo);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = gaussianLightField.gaussianSampledBLAS.buffer;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &gaussianLightField.gaussianSampledBLAS.handle);

		// Create a small scratch buffer used during build of the bottom level acceleration structure
		ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationStructureBuildGeometryInfo.dstAccelerationStructure = gaussianLightField.gaussianSampledBLAS.handle;
		accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		// BLAS Size
		blasSize = accelerationStructureBuildSizesInfo.accelerationStructureSize;

		// Build the acceleration structure on the device via a one-time command buffer submission
		// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		// Timestamp 0: BLAS build start
		vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, ASBuildTimeStampQueryPool, 0);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationStructureBuildGeometryInfo,
			&pBuildRangeInfo);

		// Timestamp 1: BLAS build end
		vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, ASBuildTimeStampQueryPool, 1);
		vulkanDevice->flushCommandBuffer(commandBuffer, graphicsQueue);

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = gaussianLightField.gaussianSampledBLAS.handle;
		gaussianLightField.gaussianSampledBLAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

		deleteScratchBuffer(scratchBuffer);
	}

	void createGaussianLightFieldTLAS() {
		VkTransformMatrixKHR transformMatrix = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f };

		VkAccelerationStructureInstanceKHR instance{};
		instance.transform = transformMatrix;
		instance.instanceCustomIndex = 0;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = gaussianLightField.gaussianSampledBLAS.deviceAddress;

		// Buffer for instance data
		vks::Buffer instancesBuffer;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&instancesBuffer,
			sizeof(VkAccelerationStructureInstanceKHR),
			&instance));

		VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
		instanceDataDeviceAddress.deviceAddress = getBufferDeviceAddress(instancesBuffer.buffer);

		VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
		accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

		// Get size info
		/*
		The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored. Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command, except that the hostAddress member of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
		*/
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

		uint32_t primitive_count = 1;

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(
			device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			&primitive_count,
			&accelerationStructureBuildSizesInfo);

		createAccelerationStructureBuffer(gaussianLightField.gaussianSampledTLAS, accelerationStructureBuildSizesInfo);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = gaussianLightField.gaussianSampledTLAS.buffer;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &gaussianLightField.gaussianSampledTLAS.handle);

		// Create a small scratch buffer used during build of the top level acceleration structure
		ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationBuildGeometryInfo.dstAccelerationStructure = gaussianLightField.gaussianSampledTLAS.handle;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;
		VkAccelerationStructureBuildRangeInfoKHR* accelerationBuildStructureRangeInfo = &accelerationStructureBuildRangeInfo;

		// TLAS size
		tlasSize = accelerationStructureBuildSizesInfo.accelerationStructureSize;

		// Build the acceleration structure on the device via a one-time command buffer submission
		// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Timestamp 2: TLAS build start
		vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, ASBuildTimeStampQueryPool, 2);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationBuildGeometryInfo,
			&accelerationBuildStructureRangeInfo);

		// Timestamp 3: TLAS build end
		vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, ASBuildTimeStampQueryPool, 3);
		vulkanDevice->flushCommandBuffer(commandBuffer, graphicsQueue);

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = gaussianLightField.gaussianSampledTLAS.handle;

		deleteScratchBuffer(scratchBuffer);
		instancesBuffer.destroy();
	}
	//light field end
#endif

	void prepare()
	{
		VulkanRTCommon::prepare();

#ifdef __ANDROID__
		vkGetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(vkGetDeviceProcAddr(device, "vkGetFenceStatus"));
		vkCmdWriteTimestamp = reinterpret_cast<PFN_vkCmdWriteTimestamp>(vkGetDeviceProcAddr(device, "vkCmdWriteTimestamp"));
#endif

		loadAssets();

		frameObjects.resize(swapChain.imageCount);

		createCommandBuffers();

		int i = 0;
		for (FrameObject& frame : frameObjects)
		{
			createBaseFrameObjects(frame, i++);
			pBaseFrameObjects.push_back(&frame);

			// Uniform buffers
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffer, sizeof(uniformDataDynamic), &uniformDataDynamic));

			VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			vulkanDevice->createAndCopyToDeviceBuffer(&uniformDataStatic, frame.uniformBufferStatic, sizeof(vks::utils::UniformDataStatic), graphicsQueue, usageFlags, memoryFlags);

			// For debugging, write hit counts
#if ENABLE_HIT_COUNTS && !RAY_QUERY
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.hitCountsbuffer, sizeof(unsigned int) * width * height, nullptr));
#endif

			// Time Stamp for measuring performance.
			setupTimeStampQueries(frame, timeStampCountPerFrame);
		}

		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &gaussianEnclosing.uniformBuffer, sizeof(vks::utils::GaussianEnclosingUniformData), nullptr));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &gaussianEnclosing.totalCounts, sizeof(unsigned int), 0));
		updateGaussianEnclosingUniformBuffer();

		// allocate device memory for vertex/index buffer
		gModel.allocateAttributeBuffers(vulkanDevice, graphicsQueue);
		// particle density
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &particleDensities, sizeof(ParticleDensity) * gModel.splatSet.size(), nullptr));
		// particle sph coefficient
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &particleSphCoefficients, sizeof(ParticleSphCoefficient) * gModel.splatSet.size(), nullptr));

		// (1) Gaussian Enclosing pass
		createGaussianEnclosingDescriptorSets();
		createGaussianEnclosingPipeline();
		computeGaussianEnclosingIcosaHedron();

		// Create the acceleration structures used to render the ray traced scene
#if LOAD_GLTF
		createBottomLevelAccelerationStructure();
		createTopLevelAccelerationStructure();
#endif

#if SPLIT_BLAS && !RAY_QUERY
		std::cout << "*** Split BLAS BEGIN ***\n";
		auto startTime = std::chrono::high_resolution_clock::now();
		splitBLAS.init(vulkanDevice);
		splitBLAS.splitBlas(gModel.vertices.storageBuffer, gModel.indices.storageBuffer, graphicsQueue);
		splitBLAS.initASBuildTimestamp(graphicsQueue);
		splitBLAS.createAS(graphicsQueue);
		auto      endTime = std::chrono::high_resolution_clock::now();
		long long loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
		std::cout << "Time spent for Split BLAS " << loadTime << "ms" << std::endl;
		std::cout << "*** Split BLAS END ***\n";
		splitBLAS.printASBuildInfo(deviceProperties);
#else
		initASBuildTimestamp();
		createBottomLevelAccelerationStructure3DGRT();
		createTopLevelAccelerationStructure3DGRT();
		printASBuildInfo();
#endif

		//gaussian light field add
#if GAUSSIAN_LIGHT_FIELD
		std::cout << "\n--- Gaussian Light Field Begin ---\n";
		startTime = std::chrono::high_resolution_clock::now();

		//calculate light sampling points and directions
		calculateGaussianLightFieldSamples();

		//image and image view set
		createGaussianLightFieldImages();
		
		//uniform buffer set
		VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		vulkanDevice->createAndCopyToDeviceBuffer(&gaussianLightField.uniformDataStatic, gaussianLightField.uniformBufferStatic, sizeof(gaussianLightField.uniformDataStatic), graphicsQueue, usageFlags, memoryFlags);

		VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &gaussianLightField.rayDirBuffer, sizeof(glm::vec2) * gaussianLightField.rayDir.size(), gaussianLightField.rayDir.data()));

		VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &gaussianLightField.rayPosBuffer, gaussianLightField.rayPos.size() * sizeof(glm::vec2), gaussianLightField.rayPos.data()));

		createGaussianLightFieldDescriptorSets();
		
		//create light sampling pipeline
		createGaussianLightFieldPipeline();
		createGuassianLightFieldShaderBindingTables();

		computeGaussianLightField();

		cleanupGaussianLightFieldComponents();
		
		auto endTime = std::chrono::high_resolution_clock::now();
		auto loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
		std::cout << "Time spent for Gaussian Light Field " << loadTime << "ms" << std::endl;
		std::cout << "--- Gaussian Light Field END ---\n";

		createGaussianLightFieldBLAS();
		createGaussianLightFieldTLAS();
		VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &gaussianLightField.gaussianSampledSphereBuffer, sizeof(GaussianSampledSphere), &gaussianLightField.gaussianSampledSphere));
		//현재 여기서 문제 발생. device local bit 인데 data를 cpu에서 gpu로 mapping 하려는 시도를 해서 그런듯 함.
#endif
		//gaussian light field end

		// (2) Particle Rendering pass
		createDescriptorSets();
		createParticleRenderingPipeline();
#if !RAY_QUERY
		createShaderBindingTables();
#endif

		prepared = true;
	}

	void draw()
	{
		FrameObject currentFrame = frameObjects[getCurrentFrameIndex()];
		VulkanRTBase::prepareFrame(currentFrame);
		updateUniformBuffer();
		VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, swapChain.buffers[currentFrame.imageIndex].view, VK_IMAGE_LAYOUT_GENERAL };
		VkWriteDescriptorSet resultImageWrite = vks::initializers::writeDescriptorSet(currentFrame.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor);
		vkUpdateDescriptorSets(device, 1, &resultImageWrite, 0, VK_NULL_HANDLE);

		buildCommandBuffer(currentFrame);
		VulkanRTBase::submitFrame(currentFrame);

#if EVAL_QUALITY
		if (evalQualFlag) {

			if (evalCameraIdx == 0)
				std::cout << "*** Evaluate quality BEGIN ***\n";

			vkQueueWaitIdle(graphicsQueue);

			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &currentFrameImg, width * height * 4, nullptr));

			vulkanDevice->copyImageToBuffer(swapChain.images[currentFrame.imageIndex], currentFrameImg, graphicsQueue, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, width, height);

			VK_CHECK_RESULT(vkMapMemory(device, currentFrameImg.memory, 0, currentFrameImg.size, 0, &currentFrameImg.mapped));

			int stride = width * 4;
			std::string fileName = "../results/evaluations/output/r_" + std::to_string(evalCameraIdx) + ".png";
			stbi_write_png(fileName.c_str(), width, height, 4, currentFrameImg.mapped, stride);

			std::cout << "\t- Camera index " << evalCameraIdx << " is completed.\n";
			evalCameraIdx++;

			vkUnmapMemory(device, currentFrameImg.memory);
			vkFreeMemory(device, currentFrameImg.memory, nullptr);

#if QUATERNION_CAMERA
			quaternionCamera.setDatasetCamera(quaternionCamera.dataType, evalCameraIdx, (float)width / height, false);
#else
			camera.setDatasetCamera(camera.dataType, evalCameraIdx, (float)width / height);
#endif
			
			if (evalCameraIdx >= quaternionCamera.getNumOfCams()) {
				evalQualFlag = false;
				std::cout << "*** Evaluate quality END ***\n";
			}
		}
#endif

#if ENABLE_HIT_COUNTS && !RAY_QUERY
		static unsigned int frame = 0;
		static bool flag = true;
		if (frame == 100 && flag) {
			vkQueueWaitIdle(graphicsQueue);
			printRayHitCounts(currentFrame);

			std::cout << "\n*** Ray hit counts END ***\n";
			flag = false;
		}
		else if (frame < 100) {
			frame++;
		}
#endif
	}

#if ENABLE_HIT_COUNTS && !RAY_QUERY
	// Print the ray hit count of each pixel of last frame to the txt file.
	void printRayHitCounts(FrameObject currentFrame) {
		uint32_t* uintData = static_cast<uint32_t*>(currentFrame.hitCountsbuffer.mapped);

		FILE* fp = fopen("../results/texts/rayHitCountsOutput.txt", "w");
		if (fp) {
			for (size_t i = 0; i < height; ++i) {
				for (size_t j = 0; j < width; ++j) {
					fprintf(fp, "%u ", uintData[i * width + j]);
				}
				fprintf(fp, "\n");
			}
			fclose(fp);
		}
	}
#endif

	virtual void render()
	{
		if (!prepared)
			return;

		draw();
	}
};

VULKAN_FULL_RT()
