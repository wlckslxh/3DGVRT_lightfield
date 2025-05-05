/*
 * Sogang University
 *
 * Graphics Lab, 2024
 *
 */

#version 460

#extension GL_EXT_shader_texture_lod : enable
#extension GL_ARB_shading_language_include : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_ray_query : enable
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_scalar_block_layout : require

#include "../base/define.glsl"
#include "../base/bufferreferences.glsl"
#include "../base/geometrytypes.glsl"
#include "../base/light.glsl"
#include "../base/pbr.glsl"

// Initialized with default value. Appropriate value will be transfered from application.
layout(constant_id = 0) const uint numOfLights = 1;	
layout(constant_id = 1) const uint numOfDynamicLights = 1;
layout(constant_id = 2) const uint numOfStaticLights = 1;
layout(constant_id = 3) const uint staticLightOffset = 1;

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerMetallicRoughness;
layout (binding = 4) uniform sampler2D samplerEmissive;
layout(binding = 5, set = 0) uniform uniformBuffer
{
	mat4 viewInverse;
	mat4 projInverse;
	Light lights[numOfDynamicLights];
} ubo;
layout(binding = 8, set = 0) uniform uniformBufferStaticLight
{
	Light lights[numOfStaticLights];
} uboStaticLight;
layout (binding = 6) uniform accelerationStructureEXT topLevelAS;
layout (binding = 7) uniform samplerCube samplerEnvMap;
layout (binding = 9) uniform sampler2D textures[];

#if SPLIT_BLAS
layout(binding = 10, set = 0) buffer Geometries{ GeometryInfo geoms[]; } geometries;
layout(binding = 11, set = 0) buffer Materials{ MaterialInfo mats[]; } materials;
#else
layout(binding = 10, set = 0) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;
#endif

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragcolor;

Triangle unpackTriangle(uint index, int vertexSize, uint64_t vertexBufferDeviceAddress,
	uint64_t indexBufferDeviceAddress, rayQueryEXT rayQuery) {
	Triangle tri;
	const uint triIndex = index * 3;

	Indices    indices = Indices(indexBufferDeviceAddress);
	Vertices   vertices = Vertices(vertexBufferDeviceAddress);

	// Unpack vertices
	// Data is packed as vec4 so we can map to the glTF vertex structure from the host side
	for (uint i = 0; i < 3; i++) {
		const uint offset = indices.i[triIndex + i] * 7;
		vec4 d0 = vertices.v[offset + 0]; // pos.xyz, n.x
		vec4 d1 = vertices.v[offset + 1]; // n.yz, uv.xy
		vec4 d2 = vertices.v[offset + 2];
		vec4 d3 = vertices.v[offset + 5];
		tri.vertices[i].pos = d0.xyz;
		tri.vertices[i].normal = vec3(d0.w, d1.xy);
		tri.vertices[i].uv = d1.zw;
		tri.vertices[i].color = d2;
		tri.vertices[i].tangent = d3;

#if SPLIT_BLAS
		vec4 d4 = vertices.v[offset + 6]; // objectID, padding
		tri.vertices[i].objectID = floatBitsToUint(d4.x);
#endif

		//tri.vertices[i].pos = vec3(transformMatrix * vec4(tri.vertices[i].pos, 1.0f));
		//tri.vertices[i].normal = vec3(transformInverseTrans * vec4(tri.vertices[i].normal, 1.0f));
	}
	// Calculate values at barycentric coordinates
	vec2 attribs = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);
	vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	tri.pos = tri.vertices[0].pos * barycentricCoords.x + tri.vertices[1].pos * barycentricCoords.y + tri.vertices[2].pos * barycentricCoords.z;
	tri.normal = normalize(tri.vertices[0].normal * barycentricCoords.x + tri.vertices[1].normal * barycentricCoords.y + tri.vertices[2].normal * barycentricCoords.z);
	tri.uv = tri.vertices[0].uv * barycentricCoords.x + tri.vertices[1].uv * barycentricCoords.y + tri.vertices[2].uv * barycentricCoords.z;
	tri.color = tri.vertices[0].color * barycentricCoords.x + tri.vertices[1].color * barycentricCoords.y + tri.vertices[2].color * barycentricCoords.z;
	tri.tangent = tri.vertices[0].tangent * barycentricCoords.x + tri.vertices[1].tangent * barycentricCoords.y + tri.vertices[2].tangent * barycentricCoords.z;
	return tri;
}

#if ANY_HIT
// for anyhit alpha testing
float getIntersectionAlpha(rayQueryEXT rayQuery) {
	const uint triIndex = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, false) * 3;	 // primitiveIndex * 3
	const vec2 barycentrics = rayQueryGetIntersectionBarycentricsEXT(rayQuery, false);

#if SPLIT_BLAS
	const uint instanceID = rayQueryGetIntersectionInstanceIdEXT(rayQuery, false);
	GeometryInfo geom = geometries.geoms[nonuniformEXT(instanceID)];

	Indices    indices = Indices(geom.indexBufferDeviceAddress);
	Vertices   vertices = Vertices(geom.vertexBufferDeviceAddress);
#else
	const uint geometryID =  rayQueryGetIntersectionGeometryIndexEXT(rayQuery, false);
	GeometryNode geometryNode = geometryNodes.nodes[nonuniformEXT(geometryID)];

	Indices    indices = Indices(geometryNode.indexBufferDeviceAddress);
	Vertices   vertices = Vertices(geometryNode.vertexBufferDeviceAddress);
#endif
	vec2 vertices_uv[3];
	vec2 uv;
	float vertices_alpha[3];
	float alpha;
#if SPLIT_BLAS
	uint objcetID;
#endif
	for (uint i = 0; i < 3; i++) {
		const uint offset = indices.i[triIndex + i] * 7;
		vec4 d1 = vertices.v[offset + 1]; 
		vec4 d2 = vertices.v[offset + 2];

		vertices_uv[i] = d1.zw;
		vertices_alpha[i] = d2.w;
#if SPLIT_BLAS
		vec4 d4 = vertices.v[offset + 6]; // objectID, padding
		objcetID = floatBitsToUint(d4.x);
#endif
	}
#if SPLIT_BLAS
	MaterialInfo geometryNode = materials.mats[nonuniformEXT(objcetID)];
#endif
	uv = vertices_uv[0] * (1.0f - barycentrics.x - barycentrics.y) + vertices_uv[1] * barycentrics.x + vertices_uv[2] * barycentrics.y;
	alpha = vertices_alpha[0] * (1.0f - barycentrics.x - barycentrics.y) + vertices_alpha[1] * barycentrics.x + vertices_alpha[2] * barycentrics.y;
	if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
		alpha = texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], uv).a;
	}

	return alpha;
}
#endif

void main() 
{
    vec4 fragPos = texture(samplerPosition, inUV).rgba;
	vec3 pos = fragPos.xyz;
	uint objectID = uint(round(fragPos.w));

    vec3 albedo = pow(texture(samplerAlbedo, inUV), vec4(2.2f)).rgb;
    vec3 aoRoughnessMetallic = texture(samplerMetallicRoughness, inUV).rgb;
    vec3 emissive = texture(samplerEmissive, inUV).rgb;
	if (emissive == vec3(1.0f)) emissive = vec3(0.0f);
	vec3 N = texture(samplerNormal, inUV).rgb;

	outFragcolor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	
	vec2 d = inUV * 2.0f - 1.0f;	// pixel position in NDC
    if(objectID-- == 0) {
		vec4 target = ubo.projInverse * vec4(d.x, d.y, 1.0f, 1.0f) ;    // (pixel position in EC) / Wc
		vec3 direction = normalize(ubo.viewInverse * vec4(normalize(target.xyz), 0.0f)).xyz;
		outFragcolor.rgb = textureLod(samplerEnvMap, direction, 0.0f).rgb;
        return;
	}

#if SPLIT_BLAS
	MaterialInfo geometryNode = materials.mats[nonuniformEXT(objectID)];
#else
    GeometryNode geometryNode = geometryNodes.nodes[nonuniformEXT(objectID)];
#endif
    if(((pushConstants.rayOption.reflection) && (geometryNode.reflectance > 0.0f)) || ((pushConstants.rayOption.refraction) && (geometryNode.refractance > 0.0f))){
		albedo.rgb = vec3(0.0f);
	}
	else {
		geometryNode.refractance = 0.0f;
		geometryNode.reflectance = 0.0f;
	}
	aoRoughnessMetallic *= vec3(1.0f, geometryNode.roughnessFactor, geometryNode.metallicFactor);
	vec3 F0 = vec3(((geometryNode.ior - 1)/(geometryNode.ior + 1)) * ((geometryNode.ior - 1)/(geometryNode.ior + 1)));
    F0 = mix(F0, albedo.rgb, aoRoughnessMetallic.b);
    vec3 V = normalize((ubo.viewInverse * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz - fragPos.xyz);

	outFragcolor.rgb += vec3(0.05f) * albedo.rgb + emissive;
	
    for(int i = 0; i < numOfLights; ++i) {
		Light light;
		if (i < staticLightOffset)
			light = ubo.lights[i];
		else 
			light = uboStaticLight.lights[i - staticLightOffset];

        rayQueryEXT shadowRayQuery;

		vec3 shadowRayDirection = light.position.xyz - fragPos.xyz;
		float tmax = length(shadowRayDirection);
		if (tmax > light.radius)
			continue;

		shadowRayDirection /= tmax;

		if (tmax >= 0.5f)
			tmax -= 0.5f;
		
		vec3 shadowRayOrigin = fragPos.xyz + SHADOW_RAY_ORIGIN_MOVEMENT_EPSILON * shadowRayDirection;
		bool shadowed = false;

		if (pushConstants.rayOption.shadowRay) {
#if ANY_HIT
			rayQueryInitializeEXT(shadowRayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 
					0xFF, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax);

			while (rayQueryProceedEXT(shadowRayQuery)) {
				if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
					if (getIntersectionAlpha(shadowRayQuery) != 0.0f) {
						rayQueryConfirmIntersectionEXT(shadowRayQuery);
					}
				}
			}
			if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
				shadowed = true;
			}
#else
			rayQueryInitializeEXT(shadowRayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT, 
					0xFF, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax);

			rayQueryProceedEXT(shadowRayQuery);

			if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)  
				shadowed = true;
#endif
		}
        if (!shadowed) {
			vec3 L = light.position.xyz - fragPos.xyz;
			vec3 radiance = ApplyAttenuation(light.color, length(L), light.radius);
			L = normalize(L);

			vec3 F = FresnelSchlick(max(dot(normalize(V + L), V), 0.0f), F0);
			vec3 specular = (DistributionGGX(N, normalize(V + L), aoRoughnessMetallic.g)
								* GeometrySmith(N, V, L, aoRoughnessMetallic.g) * F)
								/ (4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f);
        
			vec3 kD = vec3(1.0f) - F;
			kD = kD - kD * aoRoughnessMetallic.b;	        
			outFragcolor.rgb += (kD * albedo / PI + specular) * radiance * max(dot(N, L), 0.0f);
        }
    }

	const int iterations = ITERATIONS - 1;
	float iorPrev = 1.0f;
	float product = 1.0f;
	vec3 direction = -V;

	for (int iter = 0; iter < iterations; iter++)
	{
		if (geometryNode.refractance > 0.0f)
		{
			if (dot(direction, N) > 0.0f)
			{
				N *= -1.0f;
				iorPrev = geometryNode.ior;
				geometryNode.ior = 1.0f;
			}
			direction = normalize(refract(direction, N, iorPrev / geometryNode.ior));
			pos -= N * 0.01f;
			product *= geometryNode.refractance;
		}
		else if (geometryNode.reflectance > 0.0f)
		{
			direction = normalize(reflect(direction, N));
			pos += N * 0.01f;
			product *= geometryNode.reflectance;
		}
		else
			break;

		rayQueryEXT rayQuery;
        float tmax = 10000.0f;  
#if ANY_HIT
		rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsNoneEXT, 0xFF, pos, RAY_TMIN, direction, tmax);
#else
		rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, pos, RAY_TMIN, direction, tmax);
#endif

		while (rayQueryProceedEXT(rayQuery)) {
			if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)
#if ANY_HIT
				if(getIntersectionAlpha(rayQuery) != 0) { 
					rayQueryConfirmIntersectionEXT(rayQuery);
				}
#else
				rayQueryConfirmIntersectionEXT(rayQuery);
#endif
		}

		Triangle tri; 
		vec3 pbrColor = vec3(0.0f);
		if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)  
		{
			int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);

#if SPLIT_BLAS
			uint instanceID = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
			GeometryInfo geom = geometries.geoms[nonuniformEXT(instanceID)];
			tri = unpackTriangle(primitiveID, 112, geom.vertexBufferDeviceAddress, geom.indexBufferDeviceAddress, rayQuery);
			geometryNode = materials.mats[nonuniformEXT(tri.vertices[0].objectID)];
#else
			uint geometryID = rayQueryGetIntersectionGeometryIndexEXT(rayQuery, true);
			geometryNode = geometryNodes.nodes[nonuniformEXT(geometryID)];
			tri = unpackTriangle(primitiveID, 112, geometryNode.vertexBufferDeviceAddress, geometryNode.indexBufferDeviceAddress, rayQuery);
#endif

			if (geometryNode.textureIndexNormal > -1) {
				tri.normal = CalculateNormal(textures[nonuniformEXT(geometryNode.textureIndexNormal)], tri.normal, tri.uv, tri.tangent);
			}

			if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
				tri.color.rgba = pow(texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], tri.uv).rgba, vec4(2.2));
			}

			aoRoughnessMetallic = vec3(1.0f);
			if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
				aoRoughnessMetallic = texture(textures[nonuniformEXT(geometryNode.textureIndexMetallicRoughness)], tri.uv).rgb;
			}
			aoRoughnessMetallic *= vec3(1.0f, geometryNode.roughnessFactor, geometryNode.metallicFactor);

			if (nonuniformEXT(geometryNode.textureIndexEmissive) > -1) {
				pbrColor += texture(textures[nonuniformEXT(geometryNode.textureIndexEmissive)], tri.uv).rgb;
			}

			if(geometryNode.reflectance > 0.0f || geometryNode.refractance > 0.0f){
				 tri.color.rgb = vec3(0.0f);
			}

			pbrColor += vec3(0.05f) * tri.color.rgb;

			F0 = vec3(((geometryNode.ior - 1)/(geometryNode.ior + 1)) * ((geometryNode.ior - 1)/(geometryNode.ior + 1)));
			F0 = mix(F0, tri.color.rgb, aoRoughnessMetallic.b);

			for(int i = 0; i < numOfLights; ++i)
			{
				Light light; 
				if (i < staticLightOffset)
					light = ubo.lights[i];
				else 
					light = uboStaticLight.lights[i - staticLightOffset];

				rayQueryEXT shadowRayQuery;

				vec3 shadowRayDirection = light.position.xyz - tri.pos;
				float tmax = length(shadowRayDirection);

				if (tmax > light.radius)
					continue;

				shadowRayDirection /= tmax;

				if (tmax >= 0.5f)
					tmax -= 0.5f;
		
				vec3 shadowRayOrigin = tri.pos.xyz + SHADOW_RAY_ORIGIN_MOVEMENT_EPSILON * shadowRayDirection;
				bool shadowed = false;
				if (pushConstants.rayOption.shadowRay) {
#if ANY_HIT
					rayQueryInitializeEXT(shadowRayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax);
					while (rayQueryProceedEXT(shadowRayQuery)) {
						if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
							if (getIntersectionAlpha(shadowRayQuery) != 0.0f) {
								rayQueryConfirmIntersectionEXT(shadowRayQuery);
							}
						}
					}

					if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
					{
						shadowed = true;
					}
#else
					rayQueryInitializeEXT(shadowRayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT, 
							0xFF, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax);
					rayQueryProceedEXT(shadowRayQuery);

					if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)  
						shadowed = true;
#endif
				}
				if (!shadowed) {
					vec3 L = light.position.xyz - tri.pos;
					vec3 radiance = ApplyAttenuation(light.color, length(L), light.radius);
					L = normalize(L);
					V = -direction;

					vec3 F = FresnelSchlick(max(dot(normalize(V + L), V), 0.0), F0);
					vec3 specular = (DistributionGGX(tri.normal, normalize(V + L), aoRoughnessMetallic.g)
										* GeometrySmith(tri.normal, V, L, aoRoughnessMetallic.g) * F)
										/ (4.0f * max(dot(tri.normal, V), 0.0f) * max(dot(tri.normal, L), 0.0f) + 0.0001f);

					vec3 kD = vec3(1.0f) - F;

					kD = kD - kD * aoRoughnessMetallic.b; 
										
					pbrColor += (kD * tri.color.rgb / PI + specular) * radiance * max(dot(tri.normal, L), 0.0f);
				}
			}
			outFragcolor.rgb += product * pbrColor;
			pos = tri.pos;
			N = tri.normal;
		}
		else {
			outFragcolor.rgb += product * textureLod(samplerEnvMap, direction, 0.0).rgb;
			break;
		}
	}
	// Tone Mapping
	//outFragcolor.rgb = outFragcolor.rgb / (outFragcolor.rgb + vec3(1.0f));
	// Gamma Correction
	outFragcolor.rgb = pow(outFragcolor.rgb, vec3(1.0f / 2.2f));
}