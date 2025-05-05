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

layout(constant_id = 0) const uint numOfLights = 100;	

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerMetallicRoughness;
layout (binding = 4) uniform sampler2D samplerEmissive;
layout (binding = 5) uniform UBO
{
	mat4 viewInverse;
	mat4 projInverse;
    Light lights[numOfLights];
} ubo;
layout (binding = 6) uniform accelerationStructureEXT topLevelAS;
layout (binding = 7) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;
layout (binding = 8) uniform samplerCube samplerEnvMap;
layout (binding = 9) uniform sampler2D textures[];

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

    GeometryNode geometryNode = geometryNodes.nodes[objectID];
    if(((pushConstants.reflection) && (geometryNode.reflectance > 0.0f)) || ((pushConstants.refraction) && (geometryNode.refractance > 0.0f))){
		albedo.rgb = vec3(0.0f);
	}
	else {
		geometryNode.refractance = 0.0f;
		geometryNode.reflectance = 0.0f;
	}

	vec3 F0 = vec3(((geometryNode.ior - 1)/(geometryNode.ior + 1)) * ((geometryNode.ior - 1)/(geometryNode.ior + 1)));
    F0 = mix(F0, albedo.rgb, aoRoughnessMetallic.b);
    vec3 V = normalize((ubo.viewInverse * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz - fragPos.xyz);

	outFragcolor.rgb += vec3(0.05f) * albedo.rgb + emissive;
	
    for(int i = 0; i < numOfLights; ++i)
    {
        rayQueryEXT shadowRayQuery;

		vec3 shadowRayDirection = ubo.lights[i].position.xyz - fragPos.xyz;
		float tmax = length(shadowRayDirection);
		shadowRayDirection = normalize(shadowRayDirection);
		vec3 shadowRayOrigin = fragPos.xyz + SHADOW_RAY_ORIGIN_MOVEMENT_EPSILON * shadowRayDirection;
		bool shadowed = false;

		if (pushConstants.shadowRay) {
#if ANY_HIT
			rayQueryInitializeEXT(shadowRayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 
					0xFF, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax);

			while (rayQueryProceedEXT(shadowRayQuery)) {
				if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
					const uint geometryID =  rayQueryGetIntersectionGeometryIndexEXT(shadowRayQuery, false);
					const uint triIndex = rayQueryGetIntersectionPrimitiveIndexEXT(shadowRayQuery, false) * 3;	 // primitiveIndex * 3
					const vec2 barycentrics = rayQueryGetIntersectionBarycentricsEXT(shadowRayQuery, false);

					GeometryNode geometryNode = geometryNodes.nodes[nonuniformEXT(geometryID)];

					Indices    indices = Indices(geometryNode.indexBufferDeviceAddress);
					Vertices   vertices = Vertices(geometryNode.vertexBufferDeviceAddress);
					vec2 vertices_uv[3];
					vec2 uv;
					vec4 vertices_color[3];
					vec4 color;
					for (uint i = 0; i < 3; i++) {
						const uint offset = indices.i[triIndex + i] * 7;
						vec4 d1 = vertices.v[offset + 1]; 
						vec4 d2 = vertices.v[offset + 2];

						vertices_uv[i] = d1.zw;
						vertices_color[i] = d2;
					}
					uv = vertices_uv[0] * (1.0f - barycentrics.x - barycentrics.y) + vertices_uv[1] * barycentrics.x + vertices_uv[2] * barycentrics.y;
					color = vertices_color[0] * (1.0f - barycentrics.x - barycentrics.y) + vertices_color[1] * barycentrics.x + vertices_color[2] * barycentrics.y;
					if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
						color = texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], uv);
					}

					if (color.a != 0.0f) {
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
			vec3 L = ubo.lights[i].position.xyz - fragPos.xyz;
			vec3 radiance = ApplyAttenuation(ubo.lights[i].color, length(L), ubo.lights[i].radius);
			L = normalize(L);

			vec3 F    = FresnelSchlick(max(dot(normalize(V + L), V), 0.0f), F0);
			vec3 specular = ( DistributionGGX(N, normalize(V + L), aoRoughnessMetallic.g)
								* GeometrySmith(N, V, L, aoRoughnessMetallic.g) * F)
								/ (4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f);
        
			vec3 kD = vec3(1.0f) - F;
			kD = kD - kD * aoRoughnessMetallic.b;	        
			outFragcolor.rgb += (kD * albedo / PI + specular) * radiance * max(dot(N, L), 0.0f);
        }
    }

	const int iterations = ITERATIONS - 1;
	float iorPrev = 1.0f;
	float productKt = 1.0f;
	float productKr = 1.0f;
	V = -V;

	for (int iter = 0; iter < iterations; iter++)
	{
		if (geometryNode.refractance > 0.0f)
		{
			if (dot(V, N) > 0.0f)
			{
				N *= -1.0f;
				iorPrev = geometryNode.ior;
				geometryNode.ior = 1.0f;
			}
			V = normalize(refract(V, N, iorPrev / geometryNode.ior));
			pos -= N * 0.01f;
			productKt *= geometryNode.refractance;
		}
		else if (geometryNode.reflectance > 0.0f)
		{
			V = normalize(reflect(V, N));
			pos += N * 0.01f;
			productKr *= geometryNode.reflectance;
		}
		else
			break;

		rayQueryEXT rayQuery;
        float tmax = 100000.0f;  
#if ANY_HIT
		rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsNoneEXT, 0xFF, pos, RAY_TMIN, V, tmax);
#else
		rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, pos, RAY_TMIN, V, tmax);
#endif

		while (rayQueryProceedEXT(rayQuery)) {
			if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)  
				rayQueryConfirmIntersectionEXT(rayQuery);
		}

		uint geometryID;
		Triangle tri; 
		vec3 aoRoughnessMetallic;
		vec3 pbrColor = vec3(0.0f);
		if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)  
		{
			geometryID = rayQueryGetIntersectionGeometryIndexEXT(rayQuery, true);
			int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);

			geometryNode = geometryNodes.nodes[geometryID];
			tri = unpackTriangle(primitiveID, 112, geometryNode.vertexBufferDeviceAddress, geometryNode.indexBufferDeviceAddress, rayQuery);

			if (geometryNode.textureIndexNormal > -1) {
				tri.normal = CalculateNormal(textures[nonuniformEXT(geometryNode.textureIndexNormal)], tri.normal, tri.uv, tri.tangent);
			}

			if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
				tri.color.rgba = pow(texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], tri.uv).rgba, vec4(2.2));
			}
			vec3 aoRoughnessMetallic = vec3(1.0f);
			if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
				aoRoughnessMetallic = texture(textures[nonuniformEXT(geometryNode.textureIndexMetallicRoughness)], tri.uv).rgb;
			}
			aoRoughnessMetallic *= vec3(1.0f, geometryNode.roughnessFactor, geometryNode.metallicFactor);

			if(geometryNode.reflectance > 0.0f || geometryNode.refractance > 0.0f){
				 tri.color.rgb = vec3(0.0f);
			}

			pbrColor += vec3(0.05f) * tri.color.rgb +  texture(samplerEmissive, inUV).rgb; // emissive

			for(int i = 0; i < numOfLights; ++i)
			{
				rayQueryEXT shadowRayQuery;  

				vec3 shadowRayOrig = tri.pos.xyz + SHADOW_RAY_ORIGIN_MOVEMENT_EPSILON * tri.normal;
				vec3 rayDir = ubo.lights[i].position.xyz - tri.pos;
				float tmax = length(rayDir);
				rayDir = normalize(rayDir);

				rayQueryInitializeEXT(shadowRayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, shadowRayOrig, RAY_TMIN, rayDir, tmax);

				bool shadowed = false;
				if (pushConstants.shadowRay) {
#if ANY_HIT
					while (rayQueryProceedEXT(shadowRayQuery)) {
						if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
							int geometryID = rayQueryGetIntersectionGeometryIndexEXT(shadowRayQuery, false);
							int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(shadowRayQuery, false);

							GeometryNode geometryNode = geometryNodes.nodes[geometryID];
							Triangle tri = unpackTriangle(primitiveID, 112, geometryNode.vertexBufferDeviceAddress, geometryNode.indexBufferDeviceAddress, shadowRayQuery);

							if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
								tri.color.rgba = pow(texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], tri.uv).rgba, vec4(2.2));
							}
							if (tri.color.a != 0.0f) {
								rayQueryConfirmIntersectionEXT(shadowRayQuery);
							}
						}
					}

					if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
					{
						shadowed = true;
					}
#else
					if (rayQueryProceedEXT(shadowRayQuery))
					{
						shadowed = true;
					}
#endif
					if (!shadowed) {
						vec3 L = ubo.lights[i].position.xyz - tri.pos;
						vec3 radiance = ApplyAttenuation(ubo.lights[i].color, length(L), ubo.lights[i].radius);
						L = normalize(L);
								
						vec3 F = FresnelSchlick(max(dot(normalize(V + L), V), 0.0), F0);
						vec3 specular = (DistributionGGX(tri.normal, normalize(V + L), aoRoughnessMetallic.g)
											* GeometrySmith(tri.normal, V, L, aoRoughnessMetallic.g) * F)
											/ (4.0f * max(dot(tri.normal, V), 0.0f) * max(dot(tri.normal, L), 0.0f) + 0.0001f);

						vec3 kD = vec3(1.0f) - F;

						kD = kD - kD * aoRoughnessMetallic.b; 
										
						pbrColor += (kD * tri.color.rgb / PI + specular) * radiance * max(dot(tri.normal, L), 0.0f);// * spot;
					}
				}
			}
			outFragcolor.rgb += productKt * productKr * pbrColor;
			pos = tri.pos;
			N = tri.normal;
		}
		else {
			outFragcolor.rgb += productKt * productKr * textureLod(samplerEnvMap, V, 0.0).rgb;
			break;
		}
	}
	// Tone Mapping
	//outFragcolor.rgb = outFragcolor.rgb / (outFragcolor.rgb + vec3(1.0f));
	// Gamma Correction
	outFragcolor.rgb = pow(outFragcolor.rgb, vec3(1.0f / 2.2f));
}