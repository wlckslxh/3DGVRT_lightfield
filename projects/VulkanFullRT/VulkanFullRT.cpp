/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Vulkan Full Raytracing, One Blas Version.
 * 
 */

#include "VulkanRTCommon.h"
#include "VulkanUtils.h"
#include "SimpleUtils.h"

#define DIR_PATH "VulkanFullRT/"

class VulkanFullRT : public VulkanRTCommon
{
public:
#if SPLIT_BLAS
	VkTransformMatrixKHR tMat{};
	vks::Buffer tMatBuffer;
	std::vector<AccelerationStructure> splittedBLAS;

	struct GeometryInfo {
		uint64_t vertexBufferDeviceAddress;
		uint64_t indexBufferDeviceAddress;
	};
	vks::Buffer geometriesBuffer;

	struct MaterialInfo {
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
	vks::Buffer materialsBuffer;
#else
	AccelerationStructure bottomLevelAS{};

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
#endif
	AccelerationStructure topLevelAS{};

	uint32_t indexCount{ 0 };
	vks::Buffer transformBuffer;

#if DYNAMIC_SCENE
	// BLAS with Descriptions for Build
	struct BLAS_DESC
	{
		AccelerationStructure bottomLevelAS;
		VkDeviceSize blasSize;
		std::vector<VkAccelerationStructureGeometryKHR> blasGeometries;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> blasBuildRangeInfos;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> pBlasBuildRangeInfos;
		std::vector<uint32_t> maxPrimitiveCounts;
		std::vector<GeometryNode> geometryNodes;
		VkAccelerationStructureBuildSizesInfoKHR blasAccelerationStructureBuildSizesInfo{};
		ScratchBuffer blasScratchBuffer{};
		glm::mat4* pTransformMatrix;
		bool isDynamicBLAS = false;
		bool isInstancing = false;
		bool isDeformable = false;
		std::string nodeName;
	};
	std::vector<BLAS_DESC*> blasDescInstances;

	std::vector<GeometryNode> geometryNodes;

	// TLAS
	std::vector<VkAccelerationStructureInstanceKHR> asInstances{}; // BLASes
	vks::Buffer tlasBuffer;
	VkDeviceSize tlasScratchSize;
	ScratchBuffer tlasScratchBuffer{};
	bool isFirstRender = true;
#endif

	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};
	struct ShaderBindingTables {
		ShaderBindingTable raygen;
		ShaderBindingTable miss;
		ShaderBindingTable hit;
	} shaderBindingTables;

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	typedef unsigned char byte;
#endif

	vks::utils::UniformData uniformData;
	vks::utils::UniformDataStaticLight uniformDataStaticLight;

	struct SpecializationData {
		uint32_t numOfLights = NUM_OF_LIGHTS_SUPPORTED;
		uint32_t numOfDynamicLights = NUM_OF_DYNAMIC_LIGHTS;
		uint32_t numOfStaticLights = NUM_OF_STATIC_LIGHTS;
		uint32_t staticLightOffset = STATIC_LIGHT_OFFSET;
#if DYNAMIC_SCENE
		uint32_t numGeometryNodesForStatic = 0;
#endif
	} specializationData;

	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };


	vkglTF::Model scene;

	struct FrameObject : public BaseFrameObject {
#if !DIRECTRENDER
		StorageImage storageImage;
#endif
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	};

	std::vector<FrameObject> frameObjects;
	std::vector<BaseFrameObject*> pBaseFrameObjects;

	VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeatures{};
	VkPhysicalDeviceRayQueryFeaturesKHR enabledRayQueryFeatures{};
	VkPhysicalDeviceHostQueryResetFeaturesEXT physicalDeviceHostQueryResetFeatures{};

	const uint32_t timeStampCountPerFrame = 1;

	VulkanFullRT() : VulkanRTCommon()
	{
#if DYNAMIC_SCENE
		asInstances.resize(BLAS_MAX_COUNT);
#endif
		title = "Sogang Univ - Vulkan Full Ray Tracing";
		initCamera();
	}

	~VulkanFullRT()
	{
		if (device) {
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			for (FrameObject& frame : frameObjects)
			{
#if !DIRECTRENDER
				deleteStorageImage(frame.storageImage);
#endif
				frame.uniformBuffer.destroy();
				frame.uniformBufferStaticLight.destroy();
				vkDestroyQueryPool(device, frame.timeStampQueryPool, nullptr);
			}

#if SPLIT_BLAS
			for (int i = 0; i < splittedBLAS.size(); i++) {
				deleteAccelerationStructure(splittedBLAS[i]);
			}
			tMatBuffer.destroy();
			geometriesBuffer.destroy();
			materialsBuffer.destroy();
#else // !SPLIT_BLAS
			geometryNodesBuffer.destroy();
#if DYNAMIC_SCENE
			tlasBuffer.destroy();

			for (auto& blasDescInstance : blasDescInstances)
			{
				blasDescInstance->pBlasBuildRangeInfos.clear();
				blasDescInstance->blasBuildRangeInfos.clear();
				blasDescInstance->maxPrimitiveCounts.clear();
				deleteScratchBuffer(blasDescInstance->blasScratchBuffer);
				deleteAccelerationStructure(blasDescInstance->bottomLevelAS);
				delete blasDescInstance;
				blasDescInstance = nullptr;
			}
			blasDescInstances.clear();

			deleteScratchBuffer(tlasScratchBuffer);
#else // !SPLIT_BLAS && !DYNAMIC_SCENE
			deleteAccelerationStructure(bottomLevelAS);
#endif
#endif
			deleteAccelerationStructure(topLevelAS);
			transformBuffer.destroy();
			shaderBindingTables.raygen.destroy();
			shaderBindingTables.miss.destroy();
			shaderBindingTables.hit.destroy();
			destroyCommandBuffers();
		}
	}

	virtual void createCommandBuffers()
	{
		VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

		for (FrameObject& frame : frameObjects)
		{
			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &frame.commandBuffer));
		}
	}

	virtual void destroyCommandBuffers()
	{
		for (FrameObject& frame : frameObjects)
		{
			vkFreeCommandBuffers(device, cmdPool, 1, &frame.commandBuffer);
		}
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

#if DYNAMIC_SCENE
	/*
		Create the bottom level acceleration structure that contains the scene's actual geometry (vertices, triangles)
	*/
	void createBottomLevelAccelerationStructure()
	{
		bool isFirstBuild = blasDescInstances.empty();

		// Use transform matrices from the glTF nodes
		std::vector<VkTransformMatrixKHR> transformMatrices{};
		std::vector<VkTransformMatrixKHR> singleMeshBLASMats;
		uint32_t dynamicTransformStartOffset = 0; // TransformBuffer: |Static|S|S|S|S|S|S|S|S|<-Here,D|Dynamic|
		for (auto node : scene.linearNodes) {
			if (node->mesh) {
				bool isStatic = node->isStatic;
				for (auto primitive : node->mesh->primitives) {
					if (primitive->indexCount > 0) {
						VkTransformMatrixKHR transformMatrix = SimpleUtils::IdentityVkTransformMatrix;
						if (isStatic)
							transformMatrices.push_back(transformMatrix);
						else
							singleMeshBLASMats.push_back(transformMatrix);
					}
				}
			}
		}
		dynamicTransformStartOffset = transformMatrices.size();
		transformMatrices.insert(transformMatrices.end(), singleMeshBLASMats.begin(), singleMeshBLASMats.end());

		// Instance if First Build, Reference if Update
		BLAS_DESC* pSceneBlasDesc = nullptr; // Big Blas covering whole Scene
		BLAS_DESC* pHandlingBlasDesc = nullptr;
		if (isFirstBuild)
		{
			// Transform buffer
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&transformBuffer,
				static_cast<uint32_t>(transformMatrices.size()) * sizeof(VkTransformMatrixKHR),
				transformMatrices.data()));

			// Build
			pSceneBlasDesc = new BLAS_DESC{}; // as Instance
			blasDescInstances.push_back(pSceneBlasDesc);
			pHandlingBlasDesc = pSceneBlasDesc;
			pHandlingBlasDesc->pTransformMatrix = &scene.linearNodes[0]->matrix;

			uint32_t transformBufferStaticOffset = 0;
			for (auto node : scene.linearNodes) {
				if (node->mesh) {
					bool isStaticMesh = node->isStatic;
					if (isStaticMesh)
					{
						pHandlingBlasDesc = pSceneBlasDesc;
					}
					else // If Dynamic Mesh
					{
						// Make Dynamic Geometry as BLAS
						pHandlingBlasDesc = new BLAS_DESC{};
						pHandlingBlasDesc->nodeName = node->name;
						blasDescInstances.push_back(pHandlingBlasDesc); // as Instance
						pHandlingBlasDesc->pTransformMatrix = &node->mesh->uniformBlock.matrix;
					}
					for (auto primitive : node->mesh->primitives) {
						if (primitive->indexCount > 0) {
							VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
							VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
							VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

							vertexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(scene.vertices.buffer);// +primitive->firstVertex * sizeof(vkglTF::Vertex);
							indexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(scene.indices.buffer) + primitive->firstIndex * sizeof(uint32_t);
							if (isStaticMesh)
								transformBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(transformBuffer.buffer) + transformBufferStaticOffset++ * sizeof(VkTransformMatrixKHR);
							else // dynamicMesh
								transformBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(transformBuffer.buffer) + dynamicTransformStartOffset++ * sizeof(VkTransformMatrixKHR);
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

							VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
							buildRangeInfo.firstVertex = 0;
							buildRangeInfo.primitiveOffset = 0; // primitive->firstIndex * sizeof(uint32_t);
							buildRangeInfo.primitiveCount = primitive->indexCount / 3;
							buildRangeInfo.transformOffset = 0;

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

							// If Static -> Push to SceneBLAS, Else -> Push to Small BLAS
							// pHandlingBlasDesc is Referencing SceneBLAS (if Static)
							// or Referencing new Allocated Instance(for Dynamic BLAS, if Dynamic Mesh)
							pHandlingBlasDesc->blasGeometries.push_back(geometry);
							pHandlingBlasDesc->blasBuildRangeInfos.push_back(buildRangeInfo);
							pHandlingBlasDesc->maxPrimitiveCounts.push_back(primitive->indexCount / 3);

							// @todo: map material id to global texture array
							pHandlingBlasDesc->geometryNodes.push_back(geometryNode);

							if (isStaticMesh == false)
							{
								pHandlingBlasDesc->isDynamicBLAS = true;
								pHandlingBlasDesc->blasBuildRangeInfos.shrink_to_fit();
								for (auto& rangeInfo : pHandlingBlasDesc->blasBuildRangeInfos)
								{
									pHandlingBlasDesc->pBlasBuildRangeInfos.push_back(&rangeInfo);
								}
								// Writing Dynamic BLAS Complete, change Handle
								pHandlingBlasDesc = pSceneBlasDesc;
							}
						}
					}
				}
			}

			pHandlingBlasDesc->blasBuildRangeInfos.shrink_to_fit();
			for (auto& rangeInfo : pHandlingBlasDesc->blasBuildRangeInfos)
			{
				pHandlingBlasDesc->pBlasBuildRangeInfos.push_back(&rangeInfo);
			}
			// Making BLAS DESC Complete

			 //Move GeometryNode
			for (auto& blasDescInstance : blasDescInstances)
			{
				if (blasDescInstance->isDynamicBLAS == false)
					specializationData.numGeometryNodesForStatic += blasDescInstance->geometryNodes.size();

				auto moveIteratorBegin = std::make_move_iterator(blasDescInstance->geometryNodes.begin());
				auto moveIteratorEnd = std::make_move_iterator(blasDescInstance->geometryNodes.end());
				geometryNodes.insert(geometryNodes.end(), moveIteratorBegin, moveIteratorEnd);
				blasDescInstance->geometryNodes.clear();
			}


			// @todo: stage to device

			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&geometryNodesBuffer,
				static_cast<uint32_t> (geometryNodes.size() * sizeof(GeometryNode)),
				geometryNodes.data()));

		}
		else // !isFirstBuild
		{
			// Update All Node's Transform Buffer
			memcpy(
				(unsigned char*)transformBuffer.mapped, // dst
				transformMatrices.data(),
				static_cast<uint32_t>(transformMatrices.size()) * sizeof(VkTransformMatrixKHR)
			);
		}

		for (auto& refHandlingBlasDesc : blasDescInstances)
		{

			VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
			accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
			accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			accelerationStructureBuildGeometryInfo.flags =
				refHandlingBlasDesc->isDeformable ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
				: VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
			accelerationStructureBuildGeometryInfo.geometryCount = static_cast<uint32_t>(refHandlingBlasDesc->blasGeometries.size());
			accelerationStructureBuildGeometryInfo.pGeometries = refHandlingBlasDesc->blasGeometries.data();


			// First Build or Always Fast Trace
			if (isFirstBuild)
			{
				accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			}
			else
			{
				accelerationStructureBuildGeometryInfo.mode = refHandlingBlasDesc->isDeformable ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
				accelerationStructureBuildGeometryInfo.srcAccelerationStructure = refHandlingBlasDesc->bottomLevelAS.handle;
			}

			// Scratch Buffer, with Build Size
			VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
			accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
			vkGetAccelerationStructureBuildSizesKHR(
				device,
				VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
				&accelerationStructureBuildGeometryInfo,
				refHandlingBlasDesc->maxPrimitiveCounts.data(),
				&accelerationStructureBuildSizesInfo);

			VkDeviceSize scratchBufferBuildSize = std::max(accelerationStructureBuildSizesInfo.buildScratchSize, accelerationStructureBuildSizesInfo.updateScratchSize);

			if (isFirstBuild)
			{
				refHandlingBlasDesc->blasAccelerationStructureBuildSizesInfo = accelerationStructureBuildSizesInfo;
				refHandlingBlasDesc->blasSize = accelerationStructureBuildSizesInfo.accelerationStructureSize;

				createAccelerationStructureBuffer(refHandlingBlasDesc->bottomLevelAS, accelerationStructureBuildSizesInfo);

				VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
				accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
				accelerationStructureCreateInfo.buffer = refHandlingBlasDesc->bottomLevelAS.buffer;
				accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
				accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
				vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &refHandlingBlasDesc->bottomLevelAS.handle);

				// Create a small scratch buffer used during build of the bottom level acceleration structure
				refHandlingBlasDesc->blasScratchBuffer = createScratchBuffer(scratchBufferBuildSize);
			}
			else // !isFirstBuild
			{
				if (refHandlingBlasDesc->blasAccelerationStructureBuildSizesInfo.buildScratchSize < accelerationStructureBuildSizesInfo.buildScratchSize)
				{
					// May not go here, because buildScratchSize and updateScratchSize would be same
					deleteScratchBuffer(refHandlingBlasDesc->blasScratchBuffer);
					refHandlingBlasDesc->blasScratchBuffer = createScratchBuffer(refHandlingBlasDesc->blasAccelerationStructureBuildSizesInfo.updateScratchSize);
				}
			}
			accelerationStructureBuildGeometryInfo.dstAccelerationStructure = refHandlingBlasDesc->bottomLevelAS.handle;
			accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = refHandlingBlasDesc->blasScratchBuffer.deviceAddress;


			// Only build if First build. Updaing Blas is only for Deformable Object, because rigid body objects' blases are updated in TLAS with transform matrix
			if (isFirstBuild || (accelerationStructureBuildGeometryInfo.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR))
			{
				// Build the acceleration structure on the device via a one-time command buffer submission
				// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
				VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
				vkCmdBuildAccelerationStructuresKHR(
					commandBuffer,
					1,
					&accelerationStructureBuildGeometryInfo,
					refHandlingBlasDesc->pBlasBuildRangeInfos.data());
				vulkanDevice->flushCommandBuffer(commandBuffer, graphicsQueue);

				VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
				accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
				accelerationDeviceAddressInfo.accelerationStructure = refHandlingBlasDesc->bottomLevelAS.handle;
				refHandlingBlasDesc->bottomLevelAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

				if (isFirstBuild)
				{
					InstancingObject* pInstancingObj = sceneObjManager.FindAndGet_InstancingObj(refHandlingBlasDesc->nodeName);
					if (pInstancingObj)
					{
						refHandlingBlasDesc->isInstancing = true;
						pInstancingObj->blasDeviceAddress = refHandlingBlasDesc->bottomLevelAS.deviceAddress;
					}
				}
			}
		}
	}

	/*
		The top level acceleration structure contains the scene's object instances
	*/
	void createTopLevelAccelerationStructure()
	{
		bool isFirstBuild = (topLevelAS.handle == nullptr);
		if (isFirstBuild)
		{
			VkAccelerationStructureInstanceKHR asInstance{};
			uint32_t blasIndex = 0;
			// Create Default
			for (blasIndex = 0; blasIndex < blasDescInstances.size(); ++blasIndex)
			{
				asInstance.transform = SimpleUtils::glmToVkMatrix(*blasDescInstances[blasIndex]->pTransformMatrix);
				asInstance.instanceCustomIndex = blasIndex;
				asInstance.mask = blasDescInstances[blasIndex]->isInstancing ? 0 : 0xFF;
				asInstance.instanceShaderBindingTableRecordOffset = 0; // We will use the same hit group for all objects
				asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
				asInstance.accelerationStructureReference = blasDescInstances[blasIndex]->bottomLevelAS.deviceAddress;
				asInstances[blasIndex] = asInstance; // copy

				if (blasDescInstances[blasIndex]->isInstancing)
				{
					SceneObject* pSceneObj = sceneObjManager.FindAndGet_SceneObj(blasDescInstances[blasIndex]->nodeName);
					if (pSceneObj)
					{
						dynamic_cast<InstancingObject*>(pSceneObj)->blasCustomIndex = blasIndex;
					}
				}
			}
			// blas Instancing
			for (auto& pair : sceneObjManager.instancingObjects)
			{
				auto& pInstancingObj = pair.second;
				for (uint32_t i = 0; i < pInstancingObj->numInstancing; ++i)
				{
					asInstance.transform = SimpleUtils::glmToVkMatrix(pInstancingObj->instancingTransforms[i]);
					asInstance.instanceCustomIndex = pInstancingObj->blasCustomIndex;
					asInstance.mask = 0xFF;
					asInstance.instanceShaderBindingTableRecordOffset = 0;
					asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
					asInstance.accelerationStructureReference = pInstancingObj->blasDeviceAddress;
					asInstances[blasIndex++] = asInstance; // copy
				}
			}
			for (; blasIndex < BLAS_MAX_COUNT; ++blasIndex)
			{
				asInstance.transform = SimpleUtils::IdentityVkTransformMatrix;
				asInstance.mask = 0;
				asInstances[blasIndex] = asInstance; // copy
			}


			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&tlasBuffer,
				sizeof(VkAccelerationStructureInstanceKHR) * BLAS_MAX_COUNT,
				asInstances.data()));
		}
		else // !FirstBuild
		{
			uint32_t blasIndex = 0;

			// update not instancing transforms
			for (; blasIndex < blasDescInstances.size(); ++blasIndex)
			{
				asInstances[blasIndex].mask = blasDescInstances[blasIndex]->isInstancing ? 0 : 0xFF;
				asInstances[blasIndex].transform = SimpleUtils::glmToVkMatrix(*blasDescInstances[blasIndex]->pTransformMatrix);
			}

			// instancing
			for (auto& pair : sceneObjManager.instancingObjects)
			{
				auto& pInstancingObj = pair.second;

				// update instanced
				for (uint32_t i = 0; i < pInstancingObj->numInstancing; ++i)
				{
					asInstances[blasIndex].accelerationStructureReference = pInstancingObj->blasDeviceAddress;
					asInstances[blasIndex].mask = 0xFF; // activate
					asInstances[blasIndex].transform = SimpleUtils::glmToVkMatrix(pInstancingObj->instancingTransforms[i]);
					++blasIndex;
				}
				pInstancingObj->prevNumInstancing = pInstancingObj->numInstancing;
			}
			// disable others
			for (; blasIndex < BLAS_MAX_COUNT; ++blasIndex)
			{
				asInstances[blasIndex].mask = 0;
			}


			memcpy(tlasBuffer.mapped, asInstances.data(), sizeof(VkAccelerationStructureInstanceKHR) * BLAS_MAX_COUNT);
		}

		VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
		instanceDataDeviceAddress.deviceAddress = getBufferDeviceAddress(tlasBuffer.buffer);

		VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
		accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

		//uint32_t primitive_count = asInstances.size();
		uint32_t primitive_count = BLAS_MAX_COUNT;
		// Get size info
		/*
		The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored. Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command, except that the hostAddress member of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
		*/
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1; // one TLAS
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

		accelerationStructureBuildGeometryInfo.mode = isFirstBuild ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR
			: VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;

		VkAccelerationStructureBuildSizesInfoKHR tlasAccelerationStructureBuildSizesInfo{};
		tlasAccelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(
			device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			&primitive_count,
			&tlasAccelerationStructureBuildSizesInfo);

		VkDeviceSize scratchSizeMax = std::max(tlasAccelerationStructureBuildSizesInfo.buildScratchSize, tlasAccelerationStructureBuildSizesInfo.updateScratchSize);
		if (isFirstBuild)
		{
			createAccelerationStructureBuffer(topLevelAS, tlasAccelerationStructureBuildSizesInfo);

			VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
			accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
			accelerationStructureCreateInfo.buffer = topLevelAS.buffer;
			accelerationStructureCreateInfo.size = tlasAccelerationStructureBuildSizesInfo.accelerationStructureSize;
			accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
			vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &topLevelAS.handle);

			// Create a small scratch buffer used during build of the top level acceleration structure
			tlasScratchSize = scratchSizeMax;
			tlasScratchBuffer = createScratchBuffer(scratchSizeMax);
		}
		else
		{
			accelerationStructureBuildGeometryInfo.srcAccelerationStructure = topLevelAS.handle;
			if (tlasScratchSize < scratchSizeMax)
			{
				deleteScratchBuffer(tlasScratchBuffer);
				tlasScratchBuffer = createScratchBuffer(scratchSizeMax);
			}
		}
		accelerationStructureBuildGeometryInfo.dstAccelerationStructure = topLevelAS.handle;
		accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = tlasScratchBuffer.deviceAddress;

		VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

		// Build the acceleration structure on the device via a one-time command buffer submission
		// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);


		//// Make sure the copy of the instance buffer are copied before triggering the acceleration structure build
		//VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		//barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		//barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		//vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		//	VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);


		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationStructureBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
		vulkanDevice->flushCommandBuffer(commandBuffer, graphicsQueue);
	}
#else // !DYNAMIC_SCENE
	/*
	Create the bottom level acceleration structure that contains the scene's actual geometry (vertices, triangles)
	*/
#if SPLIT_BLAS
	void createGeometryMaterialBuffer()
	{
		// geometry infos: vertices and indices device address per each cells
		std::vector<GeometryInfo> geometries;
		for (int cellIdx = 0; cellIdx < scene.splittedIndices.size(); ++cellIdx) {
			GeometryInfo geometry{};
#if ONE_VERTEX_BUFFER
			geometry.vertexBufferDeviceAddress = getBufferDeviceAddress(scene.vertices.buffer);
#else
			geometry.vertexBufferDeviceAddress = getBufferDeviceAddress(scene.splittedVertices[cellIdx].buffer);
#endif
			geometry.indexBufferDeviceAddress = getBufferDeviceAddress(scene.splittedIndices[cellIdx].buffer);
			geometries.push_back(geometry);
		}
#

		// material infos: textures, shading informations
		std::vector<MaterialInfo> materials;
		for (auto node : scene.linearNodes) {
			if (node->mesh) {
				for (auto primitive : node->mesh->primitives) {
					if (primitive->indexCount > 0) {
						MaterialInfo material{};

						material.textureIndexBaseColor = primitive->material.baseColorTexture ? primitive->material.baseColorTexture->index : -1;
						material.textureIndexOcclusion = primitive->material.occlusionTexture ? primitive->material.occlusionTexture->index : -1;
						material.textureIndexNormal = primitive->material.normalTexture ? primitive->material.normalTexture->index : -1;
						material.textureIndexMetallicRoughness = primitive->material.metallicRoughnessTexture ? primitive->material.metallicRoughnessTexture->index : -1;
						material.textureIndexEmissive = primitive->material.emissiveTexture ? primitive->material.emissiveTexture->index : -1;
						material.reflectance = primitive->material.Kr;
						material.refractance = primitive->material.Kt;
						material.ior = primitive->material.ior;
						material.metallicFactor = primitive->material.metallicFactor;
						material.roughnessFactor = primitive->material.roughnessFactor;
						// @todo: map material id to global texture array
						materials.push_back(material);
					}
				}
			}
		}

		// staging buffer: for geometries and materials buffer
		VkBuffer geomStagingBuffer, matStagingBuffer;
		VkDeviceMemory geomStagingMemory, matStagingMemory;

		uint32_t geometrySize = static_cast<uint32_t> (geometries.size() * sizeof(GeometryInfo));
		uint32_t materialSize = static_cast<uint32_t> (materials.size() * sizeof(MaterialInfo));

		// geometries staging buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			geometrySize,
			&geomStagingBuffer,
			&geomStagingMemory,
			geometries.data()));

		// materials staging buffers
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			materialSize,
			&matStagingBuffer,
			&matStagingMemory,
			materials.data()));

		// geometries buffer(device local)
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&geometriesBuffer,
			geometrySize,
			(void*)nullptr));

		// materials buffer(device local)
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&materialsBuffer,
			materialSize,
			(void*)nullptr));

		// copy cmd
		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = {};

		// copy geometry staging buffer to geometry buffer
		copyRegion.size = geometrySize;
		vkCmdCopyBuffer(copyCmd, geomStagingBuffer, geometriesBuffer.buffer, 1, &copyRegion);

		// copy material staging buffer to material buffer
		copyRegion.size = materialSize;
		vkCmdCopyBuffer(copyCmd, matStagingBuffer, materialsBuffer.buffer, 1, &copyRegion);

		vulkanDevice->flushCommandBuffer(copyCmd, graphicsQueue, true);

		vkDestroyBuffer(vulkanDevice->logicalDevice, geomStagingBuffer, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, geomStagingMemory, nullptr);
		vkDestroyBuffer(vulkanDevice->logicalDevice, matStagingBuffer, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, matStagingMemory, nullptr);
	}

	void createBottomLevelAccelerationStructure(int cellIdx) {
		// Build
		// One geometry per glTF node, so we can index materials using gl_GeometryIndexEXT
		std::vector<uint32_t> maxPrimitiveCounts;
		std::vector<VkAccelerationStructureGeometryKHR> geometries;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> pBuildRangeInfos;

		VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

#if ONE_VERTEX_BUFFER
		vertexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(scene.vertices.buffer);
#else
		vertexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(scene.splittedVertices[cellIdx].buffer);
#endif
		indexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(scene.splittedIndices[cellIdx].buffer);
		transformBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(tMatBuffer.buffer);

		VkAccelerationStructureGeometryKHR geometry{};
		geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		geometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
#if ONE_VERTEX_BUFFER
		geometry.geometry.triangles.maxVertex = scene.vertices.count - 1;
#else
		geometry.geometry.triangles.maxVertex = scene.splittedVertices[cellIdx].count - 1;
#endif
		geometry.geometry.triangles.vertexStride = sizeof(vkglTF::Vertex);
		geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
		geometry.geometry.triangles.indexData = indexBufferDeviceAddress;
		geometry.geometry.triangles.transformData = transformBufferDeviceAddress;
		geometries.push_back(geometry);
		maxPrimitiveCounts.push_back(scene.splittedIndices[cellIdx].count / 3);

		VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
		buildRangeInfo.firstVertex = 0;
		buildRangeInfo.primitiveOffset = 0;
		buildRangeInfo.primitiveCount = scene.splittedIndices[cellIdx].count / 3;
		buildRangeInfo.transformOffset = 0;
		buildRangeInfos.push_back(buildRangeInfo);

		for (auto& rangeInfo : buildRangeInfos)
		{
			pBuildRangeInfos.push_back(&rangeInfo);
		}

		// Get size info
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = static_cast<uint32_t>(geometries.size());
		accelerationStructureBuildGeometryInfo.pGeometries = geometries.data();

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(
			device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			maxPrimitiveCounts.data(),
			&accelerationStructureBuildSizesInfo);

		createAccelerationStructureBuffer(splittedBLAS[cellIdx], accelerationStructureBuildSizesInfo);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = splittedBLAS[cellIdx].buffer;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &splittedBLAS[cellIdx].handle);

		// Create a small scratch buffer used during build of the bottom level acceleration structure
		ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationStructureBuildGeometryInfo.dstAccelerationStructure = splittedBLAS[cellIdx].handle;
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
		accelerationDeviceAddressInfo.accelerationStructure = splittedBLAS[cellIdx].handle;
		splittedBLAS[cellIdx].deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

		deleteScratchBuffer(scratchBuffer);
	}

	void createBottomLevelAccelerationStructures() {
		auto m = glm::mat3x4(glm::mat4(1.0f));
		memcpy(&tMat, (void*)&m, sizeof(glm::mat3x4));
		std::vector<VkTransformMatrixKHR> tMatVec;
		tMatVec.push_back(tMat);

		// Transform buffer
		//TODO: remove vector
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&tMatBuffer,
			static_cast<uint32_t>(tMatVec.size()) * sizeof(VkTransformMatrixKHR),
			tMatVec.data()));

		createGeometryMaterialBuffer();

		splittedBLAS.resize(scene.splittedIndices.size());

		for (int cellIdx = 0; cellIdx < scene.splittedIndices.size(); ++cellIdx) {
			createBottomLevelAccelerationStructure(cellIdx);
		}
	}
#else

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
#endif

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
#if SPLIT_BLAS
		for (int i = 0; i < scene.splittedIndices.size(); i++) {
			VkAccelerationStructureInstanceKHR instance{};
			instance.transform = transformMatrix;
			instance.instanceCustomIndex = instances.size();
			instance.mask = 0xFF;
			instance.instanceShaderBindingTableRecordOffset = 0;
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			instance.accelerationStructureReference = splittedBLAS[i].deviceAddress;
			instances.push_back(instance);
		}
#else
		VkAccelerationStructureInstanceKHR instance{};
		instance.transform = transformMatrix;
		instance.instanceCustomIndex = 0;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = bottomLevelAS.deviceAddress;
		instances.push_back(instance);
#endif

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

	/*
		Create the Shader Binding Tables that binds the programs and top-level acceleration structure

		SBT Layout used in VulkanFullRT project:

			/---------------------------------------\
			| Raygen                                |
			|---------------------------------------|
			| Miss(Sky Box)                         |
			|---------------------------------------|
			| Shadow Miss                           |
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
		createShaderBindingTable(shaderBindingTables.miss, 2);
		createShaderBindingTable(shaderBindingTables.hit, 2);

		// Copy handles
		// Global[0], Group Local[0]: raygen shader
		memcpy(shaderBindingTables.raygen.mapped, shaderHandleStorage.data(), handleSize);
		// Global[1 ~ 2], Group Local[0 ~ 1]: miss shader
		memcpy(shaderBindingTables.miss.mapped, shaderHandleStorage.data() + handleSizeAligned, handleSize * 2);
		// Global[3], Group Local[0]: closest hit shader
		memcpy(shaderBindingTables.hit.mapped, shaderHandleStorage.data() + handleSizeAligned * 3, handleSize * 2);
	}

	/*
		Create our ray tracing pipeline
	*/
	void createRayTracingPipeline()
	{
		// For transfer of num of lights, use specialization constant.
		std::vector<VkSpecializationMapEntry> specializationMapEntries = {
			vks::initializers::specializationMapEntry(0, 0, sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(1, sizeof(uint32_t), sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(2, sizeof(uint32_t) * 2, sizeof(uint32_t)),
			vks::initializers::specializationMapEntry(3, sizeof(uint32_t) * 3, sizeof(uint32_t)),
#if DYNAMIC_SCENE
			vks::initializers::specializationMapEntry(4, sizeof(uint32_t) * 4, sizeof(uint32_t)) // NumStaticBlas
#endif
		};
		VkSpecializationInfo specializationInfo = vks::initializers::specializationInfo(static_cast<uint32_t>(specializationMapEntries.size()), specializationMapEntries.data(), sizeof(SpecializationData), &specializationData);

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

		// For transfer push constants
		VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, sizeof(pushConstants), 0);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
		/*
			Setup ray tracing shader groups
		*/
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

#if ANY_HIT
		uint32_t anyHitIdx;
		{
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "anyhit.rahit.spv", VK_SHADER_STAGE_ANY_HIT_BIT_KHR));
			anyHitIdx = static_cast<uint32_t>(shaderStages.size()) - 1;
		}
#endif
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
			// Second shader for shadows
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroups.push_back(shaderGroup);
		}
		// Closest hit group 0 : Basic
		{
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
			shaderStages[shaderStages.size() - 1].pSpecializationInfo = &specializationInfo;
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
#if ANY_HIT
			shaderGroup.anyHitShader = anyHitIdx;
#else
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
#endif
			shaderGroups.push_back(shaderGroup);
		}
		// Closest hit group 1 : Shadow
		{
			shaderStages.push_back(loadShader(getShadersPath() + DIR_PATH + "shadow.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
#if ANY_HIT
			shaderGroup.anyHitShader = anyHitIdx;
#else
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
#endif
			shaderGroups.push_back(shaderGroup);
		}
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

	/*
		Create the descriptor sets used for the ray tracing dispatch
	*/
	void createDescriptorSets()
	{
		uint32_t imageCount = static_cast<uint32_t>(scene.textures.size());
		std::vector<VkDescriptorPoolSize> poolSizes = {
			{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 * swapChain.imageCount },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 * swapChain.imageCount },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * swapChain.imageCount },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 * swapChain.imageCount },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount * swapChain.imageCount },
#if SPLIT_BLAS
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * swapChain.imageCount },
#else
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * swapChain.imageCount },
#endif
		};
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, swapChain.imageCount);

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));	// descriptor pool

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0: Top level acceleration structure
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0),
			// Binding 1 : Ray tracing result image
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1),
			// Binding 2: Uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 2),
			// Binding 3: Cubemap sampler for miss shader
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_MISS_BIT_KHR, 3),
			// Binding 4: All images used by the glTF model
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 4, imageCount),
			// Binding 5: Uniform buffer Static light
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 5),
#if SPLIT_BLAS
			// Binding 6: Geometries information SSBO(Shader Storage Buffer Object)
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 6),
			// Binding 7: Materials information SSBO(Shader Storage Buffer Object)
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 7),
#else
			// Binding 6: Geometry node information SSBO(Shader Storage Buffer Object)
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 6),
#endif
		};

		// Unbound set
		//std::vector<VkDescriptorBindingFlagsEXT> descriptorBindingFlags = {
		//	0,
		//	0,
		//	0,
		//	0,
		//	0,
		//	0,
		//	VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT,
		//};
		std::vector<VkDescriptorBindingFlagsEXT> descriptorBindingFlags(setLayoutBindings.size(), 0);

		VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setLayoutBindingFlags{};
		setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
		setLayoutBindingFlags.bindingCount = descriptorBindingFlags.size();
		setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		descriptorSetLayoutCI.pNext = &setLayoutBindingFlags;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

		for (auto& frame : frameObjects)
		{
			VkDescriptorSet& descriptorSet = frame.descriptorSet;

			//VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableDescriptorCountAllocInfo{};
			//uint32_t variableDescCounts[] = { imageCount };
			//variableDescriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
			//variableDescriptorCountAllocInfo.descriptorSetCount = 1;
			//variableDescriptorCountAllocInfo.pDescriptorCounts = variableDescCounts;

			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			//descriptorSetAllocateInfo.pNext = &variableDescriptorCountAllocInfo;
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));	// descriptor set

			// WriteDescriptorSet for TLAS (binding0)
			VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = vks::initializers::writeDescriptorSetAccelerationStructureKHR();
			descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
			descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS.handle;

			VkWriteDescriptorSet accelerationStructureWrite{};
			accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			// The specialized acceleration structure descriptor has to be chained
			accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
			accelerationStructureWrite.dstSet = descriptorSet;
			accelerationStructureWrite.dstBinding = 0;
			accelerationStructureWrite.descriptorCount = 1;
			accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

			VkDescriptorImageInfo storageImageDescriptor;
#if DIRECTRENDER
			storageImageDescriptor = { VK_NULL_HANDLE, swapChain.buffers[frame.imageIndex].view, VK_IMAGE_LAYOUT_GENERAL };
#else
			storageImageDescriptor = { VK_NULL_HANDLE, frame.storageImage.view, VK_IMAGE_LAYOUT_GENERAL };
#endif

			// Descriptor for cube map sampler (binding 3) 
			VkDescriptorImageInfo cubeMapDescriptor = vks::initializers::descriptorImageInfo(cubeMap.sampler, cubeMap.view, cubeMap.imageLayout);

			// WriteDescriptorSet for textures of the scene
			std::vector<VkDescriptorImageInfo> textureDescriptors{};
			for (auto texture : scene.textures) {
				VkDescriptorImageInfo descriptor{};
				descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				descriptor.sampler = texture.sampler;
				descriptor.imageView = texture.view;
				textureDescriptors.push_back(descriptor);
			}

			VkWriteDescriptorSet writeDescriptorImgArray{};
			writeDescriptorImgArray.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorImgArray.dstBinding = 4;
			writeDescriptorImgArray.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorImgArray.descriptorCount = imageCount;
			writeDescriptorImgArray.dstSet = descriptorSet;
			writeDescriptorImgArray.pImageInfo = textureDescriptors.data();

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				// Binding 0: Top level acceleration structure
				accelerationStructureWrite,
				// Binding 1: Ray tracing result image
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor),
				// Binding 2: Uniform data
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &frame.uniformBuffer.descriptor),
				// Binding 3: Cubemap sampler for miss shader
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &cubeMapDescriptor),
				// Binding 4: Image descriptors for the image array
				writeDescriptorImgArray,
				// Binding 5: Uniform data Static light
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5, &frame.uniformBufferStaticLight.descriptor),
#if SPLIT_BLAS
				// Binding 6: Geometries information SSBO
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6, &geometriesBuffer.descriptor),
				// Binding 7: Materials information SSBO
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7, &materialsBuffer.descriptor),
#else
				// Binding 6: Instance information SSBO
				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6, &geometryNodesBuffer.descriptor),
#endif
			};

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
		}
	}

	/*
		If the window has been resized, we need to recreate the storage image and it's descriptor
	*/
	void handleResize()
	{
		VK_CHECK_RESULT(vkDeviceWaitIdle(device));
		for (FrameObject& frame : frameObjects) {
#if DIRECTRENDER
			VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, swapChain.buffers[frame.imageIndex].view, VK_IMAGE_LAYOUT_GENERAL };
#else
			// Recreate storage image
			createStorageImage(frame.storageImage, swapChain.colorFormat, { width, height, 1 });
			// Update descriptor using the storage image
			VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, frame.storageImage.view, VK_IMAGE_LAYOUT_GENERAL };
#endif
			VkWriteDescriptorSet resultImageWrite = vks::initializers::writeDescriptorSet(frame.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor);
			vkUpdateDescriptorSets(device, 1, &resultImageWrite, 0, VK_NULL_HANDLE);
		}
		resized = false;
	}

	/*
		Command buffer record
	*/
#if ASYNC
	void buildCommandBuffer(FrameObject& frame)
	{
		if (resized)
		{
			handleResize();
		}

		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		VK_CHECK_RESULT(vkBeginCommandBuffer(frame.commandBuffer, &cmdBufInfo));

		vkCmdResetQueryPool(frame.commandBuffer, frame.timeStampQueryPool, 0, static_cast<uint32_t>(frame.timeStamps.size()));

		/*
			Dispatch the ray tracing commands
		*/
		vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
		vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &frame.descriptorSet, 0, 0);
		vkCmdPushConstants(frame.commandBuffer, pipelineLayout, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(pushConstants), &pushConstants);

#if DIRECTRENDER
		vks::tools::setImageLayout(
			frame.commandBuffer,
			swapChain.images[frame.imageIndex],
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			subresourceRange);

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

		vks::tools::setImageLayout(
			frame.commandBuffer,
			swapChain.images[frame.imageIndex],
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			subresourceRange);
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
		/*
			Copy ray tracing output to swap chain image
		*/

		// Prepare current swap chain image as transfer destination
		vks::tools::setImageLayout(
			frame.commandBuffer,
			swapChain.images[frame.imageIndex],
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Prepare ray tracing output image as transfer source
		vks::tools::setImageLayout(
			frame.commandBuffer,
			frame.storageImage.image,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			subresourceRange);

		VkImageCopy copyRegion{};
		copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.srcOffset = { 0, 0, 0 };
		copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.dstOffset = { 0, 0, 0 };
		copyRegion.extent = { width, height, 1 };
		vkCmdCopyImage(frame.commandBuffer, frame.storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapChain.images[frame.imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// Transition swap chain image back for presentation
		vks::tools::setImageLayout(
			frame.commandBuffer,
			swapChain.images[frame.imageIndex],
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			subresourceRange);

		// Transition ray tracing output image back to general layout
		vks::tools::setImageLayout(
			frame.commandBuffer,
			frame.storageImage.image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			subresourceRange);
#endif

		drawUI(frame.commandBuffer, frameBuffers[frame.imageIndex], frame.vertexBuffer, frame.indexBuffer);

		vkCmdWriteTimestamp(frame.commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.timeStampQueryPool, 0);

		VK_CHECK_RESULT(vkEndCommandBuffer(frame.commandBuffer));
	}
#endif

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

			/*
				Dispatch the ray tracing commands
			*/
			vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
			vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &frame.descriptorSet, 0, 0);
			vkCmdPushConstants(frame.commandBuffer, pipelineLayout, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(pushConstants), &pushConstants);

#if DIRECTRENDER
			vks::tools::setImageLayout(
				frame.commandBuffer,
				swapChain.images[frame.imageIndex],
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				subresourceRange);

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

			vks::tools::setImageLayout(
				frame.commandBuffer,
				swapChain.images[frame.imageIndex],
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				subresourceRange);
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

			/*
				Copy ray tracing output to swap chain image
			*/

			// Prepare current swap chain image as transfer destination
			vks::tools::setImageLayout(
				frame.commandBuffer,
				swapChain.images[frame.imageIndex],
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				subresourceRange);

			// Prepare ray tracing output image as transfer source
			vks::tools::setImageLayout(
				frame.commandBuffer,
				frame.storageImage.image,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				subresourceRange);

			VkImageCopy copyRegion{};
			copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			copyRegion.srcOffset = { 0, 0, 0 };
			copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			copyRegion.dstOffset = { 0, 0, 0 };
			copyRegion.extent = { width, height, 1 };
			vkCmdCopyImage(frame.commandBuffer, frame.storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapChain.images[frame.imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

			// Transition swap chain image back for presentation
			vks::tools::setImageLayout(
				frame.commandBuffer,
				swapChain.images[frame.imageIndex],
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				subresourceRange);

			// Transition ray tracing output image back to general layout
			vks::tools::setImageLayout(
				frame.commandBuffer,
				frame.storageImage.image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_GENERAL,
				subresourceRange);
#endif
			drawUI(frame.commandBuffer, frameBuffers[frame.imageIndex]);

			vkCmdWriteTimestamp(frame.commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.timeStampQueryPool, 0);

			VK_CHECK_RESULT(vkEndCommandBuffer(frame.commandBuffer));
		}
	}

	void updateUniformBuffer()
	{
		uniformData.viewInverse = glm::inverse(camera.matrices.view);
		uniformData.projInverse = glm::inverse(camera.matrices.perspective);

#if ASYNC
		FrameObject currentFrame = frameObjects[getCurrentFrameIndex()];
		memcpy(currentFrame.uniformBuffer.mapped, &uniformData, sizeof(uniformData));
#endif
	}

	virtual void getEnabledFeatures()
	{
		// New Features using VkPhysicalDeviceFeatures2 structure.
		// Enable feature required for time stamp command pool reset.
		physicalDeviceHostQueryResetFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT;
		physicalDeviceHostQueryResetFeatures.hostQueryReset = VK_TRUE;

		// Enable feature required for ray query.
		enabledRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
		enabledRayQueryFeatures.rayQuery = VK_TRUE;
		enabledRayQueryFeatures.pNext = &physicalDeviceHostQueryResetFeatures;

		// Enable features required for ray tracing using feature chaining via pNext		
		enabledBufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
		enabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;
		enabledBufferDeviceAddresFeatures.pNext = &enabledRayQueryFeatures;

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

		// Original Features using VkPhysicalDeviceFeature structure.
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
		vkglTF::memoryPropertyFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		scene.loadFromFile(getAssetPath() + ASSET_PATH, vulkanDevice, graphicsQueue, glTFLoadingFlags);

		// Cubemap texture
		loadCubemap(getAssetPath() + CUBEMAP_TEXTURE_PATH, VK_FORMAT_R8G8B8A8_UNORM);
	}

#if DYNAMIC_SCENE
	void initSceneObjects()
	{
		for (auto& node : scene.linearNodes)
		{
			bool isDynamic = !node->isStatic;
			// Add Dynamic Mesh
			if (isDynamic)
			{
				glm::mat4* pCamTransform = nullptr;

				// Arguments
				DynamicObject::TYPE eType = DynamicObject::TYPE::NONE;
				if (node->name == "Cube")
				{
					eType = DynamicObject::eInstancingCube;
#if FIXED_INSTANCING
					// Destroy When Releasing ObjManager
					InstancingObject* pInstancingObj = new InstancingObject(nullptr, node->mesh, eType, g_numInstancing);
					pInstancingObj->resetInstancingPositions(node->generatePositions);
#else
					// Destroy When Releasing ObjManager
					InstancingObject* pInstancingObj = new InstancingObject(nullptr, node->mesh, eType, g_numInstancing);
					(*pInstancingObj->pTransformMat)[3].y += 1.f;
#endif

					pInstancingObj->moveDistanceMax = 0.2f;
					pInstancingObj->rotSpeed *= 3.f;

					sceneObjManager.EmplaceObject(node->name, pInstancingObj, true, true);
				}

			}
		}
	}
#endif


	bool initVulkan() {
		// Auto-compile shaders
		// Remove 'pause' from batch file for speedy execution
		system("cd ..\\shaders\\glsl\\base\\ && baseCompile.bat");
		std::cout << "\t...base project shaders compile completed.\n";
		system("cd ..\\shaders\\glsl\\VulkanFullRT\\ && VulkanFullRTCompile.bat");
		std::cout << "\t...Vulkan FullRT project shaders compile completed.\n";

		bool result = VulkanRTBase::initVulkan();
		if (!result) {
			std::cout << "VulkanFullRT initVulkan failed.\n";
			return false;
		}

		return true;
	}

	void prepare()
	{
		VulkanRTCommon::prepare();

#ifdef __ANDROID__
		vkGetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(vkGetDeviceProcAddr(device, "vkGetFenceStatus"));
		vkCmdWriteTimestamp = reinterpret_cast<PFN_vkCmdWriteTimestamp>(vkGetDeviceProcAddr(device, "vkCmdWriteTimestamp"));
#endif

		loadAssets();
#if DYNAMIC_SCENE
		initSceneObjects();
#endif
		frameObjects.resize(swapChain.imageCount);

		createCommandBuffers();

		int i = 0;
		for (FrameObject& frame : frameObjects)
		{
			createBaseFrameObjects(frame, i++);
			pBaseFrameObjects.push_back(&frame);
#if !DIRECTRENDER
			// Storage images for ray tracing output
			createStorageImage(frame.storageImage, swapChain.colorFormat, { width, height, 1 });
#endif
			// Uniform buffers
			VK_CHECK_RESULT(vulkanDevice->createAndMapBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffer, sizeof(uniformData), &uniformData));
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &frame.uniformBufferStaticLight, sizeof(vks::utils::UniformDataStaticLight), nullptr));
			vks::utils::updateLightStaticInfo(uniformDataStaticLight, frame, scene, vulkanDevice, graphicsQueue);

			// Time Stamp for measuring performance.
			setupTimeStampQueries(frame, timeStampCountPerFrame);
		}

		// Create the acceleration structures used to render the ray traced scene
#if SPLIT_BLAS
		createBottomLevelAccelerationStructures();
#else
		createBottomLevelAccelerationStructure();
#endif
		createTopLevelAccelerationStructure();

		createDescriptorSets();
		createRayTracingPipeline();
		createShaderBindingTables();

#if !ASYNC
		buildCommandBuffers();
#endif
		prepared = true;
	}

	void draw()
	{
#if ASYNC
		FrameObject currentFrame = frameObjects[getCurrentFrameIndex()];
		VulkanRTBase::prepareFrame(currentFrame);
#if DIRECTRENDER
		VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, swapChain.buffers[currentFrame.imageIndex].view, VK_IMAGE_LAYOUT_GENERAL };
		VkWriteDescriptorSet resultImageWrite = vks::initializers::writeDescriptorSet(currentFrame.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor);
		vkUpdateDescriptorSets(device, 1, &resultImageWrite, 0, VK_NULL_HANDLE);
#endif
		buildCommandBuffer(currentFrame);
		VulkanRTBase::submitFrame(currentFrame);
#else
		VulkanRTBase::prepareFrame();
		memcpy(frameObjects[acquiredIndex].uniformBuffer.mapped, &uniformData, sizeof(uniformData));
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &frameObjects[acquiredIndex].commandBuffer;
		calculateFPS(frameObjects[acquiredIndex]);
		VK_CHECK_RESULT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
		VulkanRTBase::submitFrame();
#endif
	}

	virtual void render()
	{
		if (!prepared)
			return;

#if STOP_LIGHT
		timer = 0.f;
#endif

		vks::utils::updateLightDynamicInfo(uniformData, scene, timer);
#if DYNAMIC_SCENE
		if (isFirstRender == false)
		{
			createBottomLevelAccelerationStructure();
			createTopLevelAccelerationStructure();
		}
		isFirstRender = false;
#endif
		updateUniformBuffer();

		draw();
	}
};

VULKAN_FULL_RT()
