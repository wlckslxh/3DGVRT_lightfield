/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Vulkan Hybrid Shadow map
 *
 * This closest hit shader is used for reflection or transmission.
 */

#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../base/define.glsl"
#include "../base/geometrytypes.glsl"
#include "../base/bufferreferences.glsl"
#include "../base/light.glsl"
#include "../base/pbr.glsl"

struct RayPayload {
	vec3 color;
	float dist;
	vec3 normal;
	uint objectID;
};
// Initialized with default value. Appropriate value will be transfered from application.
layout(constant_id = 0) const uint numOfLights = 1;	
layout(constant_id = 1) const uint numOfDynamicLights = 1;
layout(constant_id = 2) const uint numOfStaticLights = 1;
layout(constant_id = 3) const uint staticLightOffset = 1;
layout(constant_id = 4) const float farPlane = 1.0f;

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec2 attribs;

layout(binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0) uniform uniformBuffer
{
	mat4 viewInverse;
	mat4 projInverse;
	Light lights[numOfDynamicLights];
} ubo;
layout(binding = 10, set = 0) uniform uniformBufferStaticLight
{
	Light lights[numOfStaticLights];
} uboStaticLight;
layout(set = 0, binding = 9) uniform samplerCube pointShadowMap[numOfLights];
layout(binding = 11) uniform sampler2D textures[];
#if SPLIT_BLAS
layout(binding = 12, set = 0) buffer Geometries{ GeometryInfo geoms[]; } geometries;
layout(binding = 13, set = 0) buffer Materials{ MaterialInfo mats[]; } materials;
#else
layout(binding = 12, set = 0) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;
#endif

#include "../base/geometryfunctions.glsl"

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

bool CalculateShadow(vec3 shadowVector, uint shadowmapIdx)
{
	float curDepth = length(shadowVector);
	shadowVector = normalize(shadowVector);

	float sampledDepth = texture(pointShadowMap[nonuniformEXT(shadowmapIdx)], shadowVector).r;
	sampledDepth *= farPlane;

	bool shadowed = false;
	if (sampledDepth < curDepth - 0.1f)
	{
		shadowed = true;
	}

	return shadowed;
}

float LinearizeDepth(float depth)
{
  float n = 1.0f;
  float f = 500.0f;
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n));	
}

void main()
{
#if SPLIT_BLAS
	GeometryInfo geom = geometries.geoms[nonuniformEXT(gl_InstanceID)];

	Triangle tri = unpackTriangle(gl_PrimitiveID, 112, geom.vertexBufferDeviceAddress, geom.indexBufferDeviceAddress);

	MaterialInfo geometryNode = materials.mats[nonuniformEXT(tri.vertices[0].objectID)];
#else
	GeometryNode geometryNode = geometryNodes.nodes[nonuniformEXT(gl_GeometryIndexEXT)];

	Triangle tri = unpackTriangle(gl_PrimitiveID, 112, geometryNode.vertexBufferDeviceAddress, geometryNode.indexBufferDeviceAddress);
#endif

	#ifdef DO_NORMAL_MAPPING
	if (geometryNode.textureIndexNormal > -1) {
		tri.normal = CalculateNormal(textures[nonuniformEXT(geometryNode.textureIndexNormal)], tri.normal, tri.uv, tri.tangent);
	}
	#endif

	if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
		tri.color.rgb = pow(texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], tri.uv).rgb, vec3(2.2));
	}

	vec3 aoRoughnessMetallic = vec3(1.0f);
	if (nonuniformEXT(geometryNode.textureIndexMetallicRoughness) > -1) {
		aoRoughnessMetallic = texture(textures[nonuniformEXT(geometryNode.textureIndexMetallicRoughness)], tri.uv).rgb;
	}
	aoRoughnessMetallic *= vec3(1.0f, geometryNode.roughnessFactor, geometryNode.metallicFactor);

	rayPayload.color = vec3(0.0f);
	if (nonuniformEXT(geometryNode.textureIndexEmissive) > -1){
		rayPayload.color += texture(textures[nonuniformEXT(geometryNode.textureIndexEmissive)], tri.uv).rgb;
	}

	vec3 V = normalize((ubo.viewInverse * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz - tri.pos.xyz);

	if (geometryNode.reflectance > 0.0f || geometryNode.refractance > 0.0f) { // Kr, Kt
		tri.color.rgb = vec3(0.0f);
	}

	rayPayload.color += vec3(0.05f) * tri.color.rgb; // ambient
	rayPayload.dist = gl_RayTmaxEXT;
	rayPayload.normal = tri.normal;
#if SPLIT_BLAS
	rayPayload.objectID = tri.vertices[0].objectID;
#else
	rayPayload.objectID = gl_GeometryIndexEXT;
#endif
	
	vec3 F0 = vec3(pow((geometryNode.ior - 1.0f) / (geometryNode.ior + 1.0f), 2.0f));
	F0 = mix(F0, tri.color.rgb, aoRoughnessMetallic.b);

	vec4 ShadowCoord;
	float shadow;
	for(int i = 0; i < numOfLights; i++) {
		Light light;
		if (i < staticLightOffset)
			light = ubo.lights[i];
		else 
			light = uboStaticLight.lights[i - staticLightOffset];

		bool shadowed = false;
		if (pushConstants.rayOption.shadowRay) {
			shadowed = CalculateShadow(tri.pos.xyz - light.position.xyz, i);
		}

		if (!shadowed) {

			vec3 L = light.position.xyz - tri.pos.xyz;
			vec3 radiance = ApplyAttenuation(light.color, length(L), light.radius);
			L = normalize(L);

			vec3 F    = FresnelSchlick(max(dot(normalize(V + L), V), 0.0f), F0);
			vec3 specular = ( DistributionGGX(tri.normal, normalize(V + L), aoRoughnessMetallic.g)
									* GeometrySmith(tri.normal, V, L, aoRoughnessMetallic.g) * F )
									/ (4.0f * max(dot(tri.normal, V), 0.0f) * max(dot(tri.normal, L), 0.0f) + 0.0001f);

			vec3 kD = vec3(1.0f) - F;

			kD = kD - kD * aoRoughnessMetallic.b;	      

			rayPayload.color += (kD * tri.color.rgb / PI + specular) * radiance * max(dot(tri.normal, L), 0.0f);// * spot;
		}
    }
}
