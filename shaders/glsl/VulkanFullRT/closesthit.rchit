/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Vulkan Full Raytracing
 * 
 */

#version 460

#extension GL_EXT_ray_query : enable // ihm
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
	int effectFlag;
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
layout(location = 1) rayPayloadEXT bool shadowed;
hitAttributeEXT vec2 attribs;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;

// Initialized with default value. Appropriate value will be transfered from application.
layout(constant_id = 0) const uint numOfLights = 1;	
layout(constant_id = 1) const uint numOfDynamicLights = 1;
layout(constant_id = 2) const uint numOfStaticLights = 1;
layout(constant_id = 3) const uint staticLightOffset = 1;
#if DYNAMIC_SCENE
layout(constant_id = 4) const uint numGeometryNodesForStatic = 1;
#endif

layout(binding = 2, set = 0) uniform uniformBuffer
{
	mat4 viewInverse;
	mat4 projInverse;
	Light lights[numOfDynamicLights];
} ubo;
layout(binding = 5, set = 0) uniform uniformBufferStaticLight
{
	Light lights[numOfStaticLights];
} uboStaticLight;

layout(binding = 4, set = 0) uniform sampler2D textures[];

#if SPLIT_BLAS
layout(binding = 6, set = 0) buffer Geometries{ GeometryInfo geoms[]; } geometries;
layout(binding = 7, set = 0) buffer Materials{ MaterialInfo mats[]; } materials;
#else
layout(binding = 6, set = 0) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;
#endif

#include "../base/geometryfunctions.glsl"

void main()
{

#if SPLIT_BLAS
	GeometryInfo geom = geometries.geoms[nonuniformEXT(gl_InstanceID)];

	Triangle tri = unpackTriangle(gl_PrimitiveID, 112, geom.vertexBufferDeviceAddress, geom.indexBufferDeviceAddress);// 4 * 4 * 7(vec4 * float * Vertex size)

	MaterialInfo mat = materials.mats[nonuniformEXT(tri.vertices[0].objectID)];

	#ifdef DO_NORMAL_MAPPING
	if (mat.textureIndexNormal > -1) {
		tri.normal = CalculateNormal(textures[nonuniformEXT(mat.textureIndexNormal)], tri.normal, tri.uv, tri.tangent);
	}
	#endif

	if (nonuniformEXT(mat.textureIndexBaseColor) > -1) {
		tri.color.rgb = pow(texture(textures[nonuniformEXT(mat.textureIndexBaseColor)], tri.uv).rgb, vec3(2.2f));
	}

	vec3 aoRoughnessMetallic = vec3(1.0f);
	if (nonuniformEXT(mat.textureIndexMetallicRoughness) > -1) {
		aoRoughnessMetallic = texture(textures[nonuniformEXT(mat.textureIndexMetallicRoughness)], tri.uv).rgb;
	}
	aoRoughnessMetallic *= vec3(1.0f, mat.roughnessFactor, mat.metallicFactor);

	rayPayload.color = vec3(0.0f);
	if (nonuniformEXT(mat.textureIndexEmissive) > -1){
		rayPayload.color += texture(textures[nonuniformEXT(mat.textureIndexEmissive)], tri.uv).rgb;
	}

	rayPayload.effectFlag = 0;
	if ((mat.reflectance > 0.0f) && pushConstants.rayOption.reflection) { // Kr
		rayPayload.effectFlag = 1 + int(tri.vertices[0].objectID);
		tri.color.rgb = vec3(0.0f);
	}
	else if ((mat.refractance > 0.0f) && pushConstants.rayOption.refraction) { // Kt
		rayPayload.effectFlag = -1 - int(tri.vertices[0].objectID);
		tri.color.rgb = vec3(0.0f);
	}

	vec3 F0 = vec3(((mat.ior - 1.0f) / (mat.ior + 1.0f)) * ((mat.ior - 1.0f) / (mat.ior + 1.0f)));
#else
	uint nodeIndex;
#if DYNAMIC_SCENE
	uint instanceIndex = gl_InstanceCustomIndexEXT;
	nodeIndex = (instanceIndex == 0) // is Static?
		? gl_GeometryIndexEXT
		: numGeometryNodesForStatic + (instanceIndex - 1);
#else
	nodeIndex = gl_GeometryIndexEXT;
#endif
	GeometryNode geometryNode = geometryNodes.nodes[nonuniformEXT(nodeIndex)];

	Triangle tri = unpackTriangle(gl_PrimitiveID, 112, geometryNode.vertexBufferDeviceAddress, geometryNode.indexBufferDeviceAddress);

#if DYNAMIC_SCENE
	mat4 worldMat = mat4(gl_ObjectToWorldEXT);
	mat3 normalMatrix = transpose(inverse(mat3(worldMat)));
	tri.normal = normalize(normalMatrix * tri.normal);
	tri.pos = (worldMat * vec4(tri.pos, 1.0)).xyz;
#endif


	if (geometryNode.textureIndexNormal > -1) {
		tri.normal = CalculateNormal(textures[nonuniformEXT(geometryNode.textureIndexNormal)], tri.normal, tri.uv, tri.tangent);
	}
	

	if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
		tri.color.rgb = pow(texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], tri.uv).rgb, vec3(2.2f));
	}

	vec3 aoRoughnessMetallic = vec3(1.0f);
	if (geometryNode.textureIndexMetallicRoughness > -1) {
		aoRoughnessMetallic = texture(textures[nonuniformEXT(geometryNode.textureIndexMetallicRoughness)], tri.uv).rgb;
	}
	aoRoughnessMetallic *= vec3(1.0f, geometryNode.roughnessFactor, geometryNode.metallicFactor);

	rayPayload.color = vec3(0.0f);
	if (geometryNode.textureIndexEmissive > -1){
		rayPayload.color += texture(textures[nonuniformEXT(geometryNode.textureIndexEmissive)], tri.uv).rgb;
	}

	rayPayload.effectFlag = 0;
	if ((geometryNode.reflectance > 0.0f && pushConstants.rayOption.reflection)) { // Kr 
		rayPayload.effectFlag = 1 + int(nodeIndex);
		tri.color.rgb = vec3(0.0f);
	}
	else if ((geometryNode.refractance > 0.0f) && pushConstants.rayOption.refraction) { // Kt
		rayPayload.effectFlag = -1 - int(nodeIndex);
		tri.color.rgb = vec3(0.0f);
	}
		

	vec3 F0 = vec3(((geometryNode.ior - 1.0f) / (geometryNode.ior + 1.0f)) * ((geometryNode.ior - 1.0f) / (geometryNode.ior + 1.0f)));
#endif
	F0 = mix(F0, tri.color.rgb, aoRoughnessMetallic.b);
	
	vec3 V = normalize(vec3(ubo.viewInverse * vec4(0.0f, 0.0f, 0.0f, 1.0f)) - tri.pos);
	rayPayload.color += vec3(0.05f) * tri.color.rgb; // ambient

	for(int i = 0; i < numOfLights; i++) {
		Light light;
		if (i < staticLightOffset)
			light = ubo.lights[i];
		else 
			light = uboStaticLight.lights[i - staticLightOffset];

		vec3 shadowRayDirection = light.position.xyz - tri.pos.xyz;
		float tmax = length(shadowRayDirection);
		if (tmax > light.radius)
			continue;
		shadowRayDirection /= tmax;

		if (tmax >= 0.5f)
			tmax -= 0.5f;
		
		vec3 shadowRayOrigin = tri.pos + SHADOW_RAY_ORIGIN_MOVEMENT_EPSILON * shadowRayDirection;
		shadowed = false;
		if (pushConstants.rayOption.shadowRay) {
#ifndef USE_RAY_QUERY
			shadowed = true;
#if ANY_HIT
			traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax, 1);
#else
			traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT, 0xFF, 1, 0, 1, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax, 1);
#endif
#else
			rayQueryEXT shadowRayQuery;
#if ANY_HIT
			rayQueryInitializeEXT(shadowRayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 
				0xFF, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax);
			while (rayQueryProceedEXT(shadowRayQuery)) {
				if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
					const uint triIndex = rayQueryGetIntersectionPrimitiveIndexEXT(shadowRayQuery, false) * 3;	 // primitiveIndex * 3
					const vec2 barycentrics = rayQueryGetIntersectionBarycentricsEXT(shadowRayQuery, false);

#if SPLIT_BLAS
					const uint instanceID = rayQueryGetIntersectionInstanceIdEXT(shadowRayQuery, false);
					GeometryInfo geom = geometries.geoms[nonuniformEXT(instanceID)];

					Indices    indices = Indices(geom.indexBufferDeviceAddress);
					Vertices   vertices = Vertices(geom.vertexBufferDeviceAddress);
#else
					const uint geometryID =  rayQueryGetIntersectionGeometryIndexEXT(shadowRayQuery, false);
					GeometryNode geometryNode = geometryNodes.nodes[nonuniformEXT(geometryID)];

					Indices    indices = Indices(geometryNode.indexBufferDeviceAddress);
					Vertices   vertices = Vertices(geometryNode.vertexBufferDeviceAddress);
#endif
		
					vec2 vertices_uv[3];
					vec2 uv;
					vec4 vertices_color[3];
					vec4 color;
#if SPLIT_BLAS
					uint objcetID;
#endif
					for (uint i = 0; i < 3; i++) {
						const uint offset = indices.i[triIndex + i] * 7;
						vec4 d1 = vertices.v[offset + 1]; 
						vec4 d2 = vertices.v[offset + 2];

						vertices_uv[i] = d1.zw;
						vertices_color[i] = d2;
#if SPLIT_BLAS
						vec4 d4 = vertices.v[offset + 6]; // objectID, padding
						objcetID = floatBitsToUint(d4.x);
#endif
					}
					uv = vertices_uv[0] * (1.0f - barycentrics.x - barycentrics.y) + vertices_uv[1] * barycentrics.x + vertices_uv[2] * barycentrics.y;
					color = vertices_color[0] * (1.0f - barycentrics.x - barycentrics.y) + vertices_color[1] * barycentrics.x + vertices_color[2] * barycentrics.y;
					
#if SPLIT_BLAS
					MaterialInfo mat = materials.mats[nonuniformEXT(objcetID)];
					if (nonuniformEXT(mat.textureIndexBaseColor) > -1) {
						color = texture(textures[nonuniformEXT(mat.textureIndexBaseColor)], uv);
					}
#else
					if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
						color = texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], uv);
					}
#endif
					if (color.a != 0.0f) {
						rayQueryConfirmIntersectionEXT(shadowRayQuery);
					}
				}

				if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
					shadowed = true;
				}
			}
#else
			rayQueryInitializeEXT(shadowRayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT, 
				0xFF, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax);
			rayQueryProceedEXT(shadowRayQuery);

			if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)  
				shadowed = true;
#endif
#endif
		}
		if (!shadowed) {
			vec3 V = normalize(vec3(ubo.viewInverse[3].xyz - tri.pos));
			vec3 L = light.position.xyz - tri.pos;
			vec3 radiance = ApplyAttenuation(light.color, length(L), light.radius);
			L = normalize(L);

			float dotNL = max(dot(tri.normal, shadowRayDirection), 0.0f);
			float dotNV = max(dot(tri.normal, V), 0.0f);
			vec3 H = normalize(V + shadowRayDirection);

			vec3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);
			vec3 specular = ( DistributionGGX(tri.normal, H, aoRoughnessMetallic.g)
									* GeometrySmith(dotNV, dotNL, aoRoughnessMetallic.g) * F)
									/ (4.0f * dotNV * dotNL + 0.0001f);

			vec3 kD = vec3(1.0f) - F;	// kS = F
	
			kD = kD - kD * aoRoughnessMetallic.b;	       

			rayPayload.color += (kD * tri.color.rgb / PI + specular) * radiance * dotNL; // direct lighting
		}
    }
 
	rayPayload.dist = gl_RayTmaxEXT;
	rayPayload.normal = tri.normal;
}
