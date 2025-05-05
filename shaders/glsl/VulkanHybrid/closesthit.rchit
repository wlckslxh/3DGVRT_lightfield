/*
 * Sogang University
 *
 * Graphics Lab, 2024
 *
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

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
layout(location = 1) rayPayloadEXT bool shadowed;

// Initialized with default value. Appropriate value will be transfered from application.
layout(constant_id = 0) const uint numOfLights = 1;	
layout(constant_id = 1) const uint numOfDynamicLights = 1;
layout(constant_id = 2) const uint numOfStaticLights = 1;
layout(constant_id = 3) const uint staticLightOffset = 1;

hitAttributeEXT vec2 attribs;

layout(binding = 0) uniform accelerationStructureEXT topLevelAS;

layout(binding = 2, set = 0) uniform uniformBuffer
{
	mat4 viewInverse;
	mat4 projInverse;
	Light lights[numOfDynamicLights];
} ubo;
layout(binding = 9, set = 0) uniform uniformBufferStaticLight
{
	Light lights[numOfStaticLights];
} uboStaticLight;
layout(binding = 10) uniform sampler2D textures[];

#if SPLIT_BLAS
layout(binding = 11, set = 0) buffer Geometries{ GeometryInfo geoms[]; } geometries;
layout(binding = 12, set = 0) buffer Materials{ MaterialInfo mats[]; } materials;
#else
layout(binding = 11, set = 0) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;
#endif

#include "../base/geometryfunctions.glsl"

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

	vec3 V = normalize((ubo.viewInverse * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz - tri.pos);

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

	for(int i = 0; i < numOfLights; i++) {
		Light light;
		if (i < staticLightOffset)
			light = ubo.lights[i];
		else 
			light = uboStaticLight.lights[i - staticLightOffset];

		shadowed = false;
		if (pushConstants.rayOption.shadowRay) {

			vec3 shadowRayDirection = light.position.xyz - tri.pos;
			float tmax = length(shadowRayDirection);
			if (tmax > light.radius)
				continue;

			shadowRayDirection /= tmax;
			if (tmax >= 0.5f)
				tmax -= 0.5f;

			vec3 shadowRayOrigin = tri.pos + shadowRayDirection * SHADOW_RAY_ORIGIN_MOVEMENT_EPSILON;
			shadowed = true;

#if ANY_HIT
				traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax, 1);
#else
				traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT, 0xFF, 1, 0, 1, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax, 1);
#endif
		}
		if (!shadowed) {
			vec3 L = light.position.xyz - tri.pos;
			vec3 radiance = ApplyAttenuation(light.color, length(L), light.radius);
			L = normalize(L);

			vec3 F    = FresnelSchlick(max(dot(normalize(V + L), V), 0.0f), F0);
			vec3 specular = ( DistributionGGX(tri.normal, normalize(V + L), aoRoughnessMetallic.g)
									* GeometrySmith(tri.normal, V, L, aoRoughnessMetallic.g) * F )
									/ (4.0f * max(dot(tri.normal, V), 0.0f) * max(dot(tri.normal, L), 0.0f) + 0.0001f);
           
			vec3 kD = vec3(1.0f) - F;

			kD = kD - kD * aoRoughnessMetallic.b;	      

			rayPayload.color += (kD * tri.color.rgb / PI + specular) * radiance * max(dot(tri.normal, L), 0.0f);
		}
    }
}
