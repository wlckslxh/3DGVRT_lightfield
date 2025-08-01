/*
* Vulkan glTF model and texture loading class based on tinyglTF (https://github.com/syoyo/tinygltf)
*
* Copyright (C) 2018-2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

/*
 * Note that this isn't a complete glTF loader and not all features of the glTF 2.0 spec are supported
 * For details on how glTF 2.0 works, see the official spec at https://github.com/KhronosGroup/glTF/tree/master/specification/2.0
 *
 * If you are looking for a complete glTF implementation, check out https://github.com/SaschaWillems/Vulkan-glTF-PBR/
 */

#pragma once

#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanDevice.h"
#include "VulkanRTCommon.h"

#include <ktx.h>
#include <ktxvulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#ifdef VK_USE_PLATFORM_ANDROID_KHR
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif
#include "tiny_gltf.h"

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#endif

#include "Define.h"

namespace vkglTF
{
	static constexpr uint32_t skinnedMeshMaxNum = 20;

	enum DescriptorBindingFlags {
		ImageBaseColor = 0x00000001,
		ImageNormalMap = 0x00000002
	};

	extern VkDescriptorSetLayout descriptorSetLayoutImage;
	extern VkDescriptorSetLayout descriptorSetLayoutUbo;
	extern VkMemoryPropertyFlags bufferUsageFlags;
	extern uint32_t descriptorBindingFlags;

	struct CompressionFormat {
		ktx_transcode_fmt_e ktxFomrat;
		VkFormat vulkanFormat;
	};

	struct Node;

	typedef struct UniformBlock {
		alignas(16)
		glm::mat4 matrix;
		glm::mat4 jointMatrix[64]{};
		float jointcount{ 0 };
		float padding[3]{};
	}UniformBlock, StorageSingleBlock;

	/*
		glTF texture loading class
	*/
	struct Texture {
		vks::VulkanDevice* device = nullptr;
		VkImage image;
		VkImageLayout imageLayout;
		VkDeviceMemory deviceMemory;
		VkImageView view;
		uint32_t width, height;
		uint32_t mipLevels;
		uint32_t layerCount;
		VkDescriptorImageInfo descriptor;
		VkSampler sampler;
		int32_t index;
		void updateDescriptor();
		void destroy();
		void fromglTfImage(tinygltf::Image& gltfimage, std::string path, vks::VulkanDevice* device, VkQueue copyQueue, CompressionFormat targetFormat);
	};

	/*
		glTF material class
	*/
	struct Material {
		vks::VulkanDevice* device = nullptr;
		enum AlphaMode { ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };
		AlphaMode alphaMode = ALPHAMODE_OPAQUE;
		float alphaCutoff = 1.0f;
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		float Kr = 0.0f;
		float Kt = 0.0f;
		float ior = 1.5f;
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		vkglTF::Texture* baseColorTexture = nullptr;
		vkglTF::Texture* metallicRoughnessTexture = nullptr;
		vkglTF::Texture* normalTexture = nullptr;
		vkglTF::Texture* occlusionTexture = nullptr;
		vkglTF::Texture* emissiveTexture = nullptr;

		vkglTF::Texture* specularGlossinessTexture;
		vkglTF::Texture* diffuseTexture;

		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

		Material(vks::VulkanDevice* device) : device(device) {};
		void createDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, uint32_t descriptorBindingFlags);
	};

	/*
		glTF Light class
	*/
	enum LightType {
		//POINT_LIGHT,
		//DIRECTIONAL_LIGHT,
		//SPOT_LIGHT,
		STATIC_LIGHT,
		DYNAMIC_LIGHT,
		LIGHT_TYPE_NONE
	};

	struct SpotLight {
		float innerConeAngle = 0.0f;
		float outerConeAngle = 0.0f;
	};

	struct Light {
		vks::VulkanDevice* device = nullptr;

		glm::vec3 color = glm::vec3(0.0f, 0.0f, 0.0f);
		LightType type = LIGHT_TYPE_NONE;
		SpotLight spot = { 0.0f, 0.0f };
		float intensity = 1.0f;
		float radius = 1.0f;
		glm::mat4 matrix;

		Light(vks::VulkanDevice* device) : device(device) {};
	};

	/*
		glTF primitive
	*/
	struct Primitive {
		uint32_t firstIndex;
		uint32_t indexCount;
		uint32_t firstVertex;
		uint32_t vertexCount;
		Material& material;

		struct Dimensions {
			glm::vec3 min = glm::vec3(FLT_MAX);
			glm::vec3 max = glm::vec3(-FLT_MAX);
			glm::vec3 size;
			glm::vec3 center;
			float radius;
		} dimensions;

		void setDimensions(glm::vec3 min, glm::vec3 max);
		Primitive(uint32_t firstIndex, uint32_t indexCount, Material& material) : firstIndex(firstIndex), indexCount(indexCount), material(material) {};
	};

	/*
		glTF mesh
	*/
	struct Mesh {
		vks::VulkanDevice* device;

		std::vector<Primitive*> primitives;
		std::string name;
		uint32_t objectID;

#if USE_ANIMATION
		struct UniformBuffer {
			VkBuffer buffer;
			VkDeviceMemory memory;
			VkDescriptorBufferInfo descriptor;
			VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
			void* mapped;
		} uniformBuffer;
#endif

		UniformBlock uniformBlock{};

		Mesh(vks::VulkanDevice* device, glm::mat4 matrix);
		~Mesh();
	};

	/*
		glTF skin
	*/
	struct Skin {
		std::string name;
		Node* skeletonRoot = nullptr;
		std::vector<glm::mat4> inverseBindMatrices;
		std::vector<Node*> joints;
	};

	/*
		glTF node
	*/
	struct Node {
		Node* parent;
		uint32_t index;
		std::vector<Node*> children;
		glm::mat4 matrix;
		std::string name;
		Mesh* mesh;
#if USE_ANIMATION
		Skin* skin;
#endif
		int32_t skinIndex = -1;
		glm::vec3 translation{};
		glm::vec3 scale{ 1.0f };
		glm::quat rotation{};
		glm::mat4 localMatrix();
		glm::mat4 getMatrix();
		glm::mat4 localRotationMatrix();
		glm::mat4 getRotationMatrix();

		int32_t blasIndex;

		void update(); // Update only Uniform Buffer, not member "matrix".
		~Node();
	};

	/*
		glTF animation channel
	*/
	struct AnimationChannel {
		enum PathType { TRANSLATION, ROTATION, SCALE };
		PathType path;
		Node* node;
		uint32_t samplerIndex;
	};

	/*
		glTF animation sampler
	*/
	struct AnimationSampler {
		enum InterpolationType { LINEAR, STEP, CUBICSPLINE };
		InterpolationType interpolation;
		std::vector<float> inputs;
		std::vector<glm::vec4> outputsVec4;
	};

	/*
		glTF animation
	*/
	struct Animation {
		std::string name;
		std::vector<AnimationSampler> samplers;
		std::vector<AnimationChannel> channels;
		float start = std::numeric_limits<float>::max();
		float end = std::numeric_limits<float>::min();
	};

	/*
		glTF default vertex layout with easy Vulkan mapping functions
	*/
	enum class VertexComponent { Position, Normal, UV, Color, Tangent, Joint0, Weight0, ObjectID };
	
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	typedef unsigned char byte;
#endif

	typedef struct PaddedUint32 {
		uint32_t data;
		byte padding[12];
	}paddeduint32;

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec4 color;
		glm::vec4 joint0;
		glm::vec4 weight0;
		glm::vec4 tangent;
		paddeduint32 objectID;
		static VkVertexInputBindingDescription vertexInputBindingDescription;
		static std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;
		static VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
		static VkVertexInputBindingDescription inputBindingDescription(uint32_t binding);
		static VkVertexInputAttributeDescription inputAttributeDescription(uint32_t binding, uint32_t location, VertexComponent component);
		static std::vector<VkVertexInputAttributeDescription> inputAttributeDescriptions(uint32_t binding, const std::vector<VertexComponent> components);
		/** @brief Returns the default pipeline vertex input state create info structure for the requested vertex components */
		static VkPipelineVertexInputStateCreateInfo* getPipelineVertexInputState(const std::vector<VertexComponent> components);
	};

	enum FileLoadingFlags {
		None = 0x00000000,
		PreTransformVertices = 0x00000001,
		PreMultiplyVertexColors = 0x00000002,
		FlipY = 0x00000004,
		DontLoadImages = 0x00000008
	};

	enum RenderFlags {
		BindImages = 0x00000001,
		RenderOpaqueNodes = 0x00000002,
		RenderAlphaMaskedNodes = 0x00000004,
		RenderAlphaBlendedNodes = 0x00000008
	};

	/*
		glTF model loading and rendering class
	*/
	class Model {
	private:
		vkglTF::Texture* getTexture(uint32_t index);
		void createEmptyTexture(VkQueue transferQueue);
	public:
		vkglTF::Texture emptyTexture;
		int geometryNum = 0;
		int instanceNum = 0;
		int primitiveNum = 0;

		vks::VulkanDevice* device;
		VkDescriptorPool descriptorPool;

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

		std::vector<Node*> nodes;
		std::vector<Node*> linearNodes;

		std::vector<Skin*> skins;

		std::vector<Texture> textures;
		std::vector<Material> materials;
		std::vector<Animation> animations;

		std::vector<Light> lights;

		struct Dimensions {
			glm::vec3 min = glm::vec3(FLT_MAX);
			glm::vec3 max = glm::vec3(-FLT_MAX);
			glm::vec3 size;
			glm::vec3 center;
			float radius;
		} dimensions;

		bool metallicRoughnessWorkflow = true;
		bool buffersBound = false;
		std::string path;

		Model() {};
		~Model();
		void loadNode(vkglTF::Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalscale);
		void loadSkins(tinygltf::Model& gltfModel);
		void loadImages(tinygltf::Model& gltfModel, vks::VulkanDevice* device, VkQueue transferQueue);
		void loadMaterials(tinygltf::Model& gltfModel);
		void loadLights(tinygltf::Model& gltfModel);
		void loadAnimations(tinygltf::Model& gltfModel);

		void saveGeometries(std::vector<Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer, VkQueue transferQueue);

		void loadFromFile(std::string filename, vks::VulkanDevice* device, VkQueue transferQueue, uint32_t fileLoadingFlags = vkglTF::FileLoadingFlags::None, float scale = 1.0f);
		void bindBuffers(VkCommandBuffer commandBuffer);
		void drawNode(Node* node, VkCommandBuffer commandBuffer, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1);
		void draw(VkCommandBuffer commandBuffer, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1);
		void getNodeDimensions(Node* node, glm::vec3& min, glm::vec3& max);
		void getSceneDimensions();
		void updateAnimation(uint32_t index, float time);
		Node* findNode(Node* parent, uint32_t index);
		Node* nodeFromIndex(uint32_t index);
		void prepareNodeDescriptor(vkglTF::Node* node, VkDescriptorSetLayout descriptorSetLayout);
	};
}