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
layout(constant_id = 1) const uint numOfPointLights = 1;

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerMetallicRoughness;
layout (binding = 4) uniform sampler2D samplerEmissive;
layout (binding = 5) uniform UBO
{
	mat4 modelViewProjectionMatrix;
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

float CalculateSpotLight(vec3 pos, Light light)
{
    // new updating version
    float lightAngleScale = 1.0f / max(0.001f, cos(light.spot.innerConeAngle) - cos(light.spot.outerConeAngle));
    float lightAngleOffset = -cos(light.spot.outerConeAngle) * lightAngleScale;

    float spotFactor = dot(spotDir, normalize(pos - light.position.xyz));
    float angularAttenuation = clamp(spotFactor * lightAngleScale + lightAngleOffset, 0.0f, 1.0f); 
    angularAttenuation *= angularAttenuation;
    return angularAttenuation;
}

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
	uint objectID = uint(round(fragPos.w));
    vec3 N = texture(samplerNormal, inUV).rgb;
    vec4 albedo = pow(texture(samplerAlbedo, inUV), vec4(2.2f));

    vec3 aoRoughnessMetallic = texture(samplerMetallicRoughness, inUV).rgb;
    vec3 emissive = texture(samplerEmissive, inUV).rgb;
    if (emissive == vec3(1.0f)) emissive = vec3(0.0f);

	outFragcolor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	
	vec2 d = inUV * 2.0f - 1.0f;	// pixel position in NDC
    if(objectID-- == 0) {
		vec4 target = ubo.projInverse * vec4(d.x, d.y, 1.0f, 1.0f) ;    // (pixel position in EC) / Wc
		vec3 direction = normalize(ubo.viewInverse * vec4(normalize(target.xyz), 0.0f)).xyz;
		outFragcolor.rgb = textureLod(samplerEnvMap, direction, 0.0f).rgb;
        return;
	}

    GeometryNode geometryNode = geometryNodes.nodes[objectID];
    float Kr = geometryNode.reflectance;
    float Kt = geometryNode.refractance;
    if(Kr>0.0f || Kt>0.0f){
		albedo.rgb=vec3(0.0f);
	}

    float ior = geometryNode.ior;
	//vec3 F0 = vec3(pow((ior - 1) / (ior + 1), 2));
	vec3 F0 = vec3(((ior - 1) / (ior + 1))*((ior - 1) / (ior + 1)));
    F0 = mix(F0, albedo.rgb, aoRoughnessMetallic.g);
    vec3 V = normalize((ubo.viewInverse * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz - fragPos.xyz);

	outFragcolor.rgb += vec3(0.05f) * albedo.rgb; // ambienrt
	outFragcolor.rgb += emissive;	// emissive
	
    for(int i = 0; i < numOfLights; ++i)
    {
        rayQueryEXT shadowRayQuery;

        vec3 shadowRayOrig = fragPos.xyz + SHADOW_RAY_ORIGIN_MOVEMENT_EPSILON * N;

        vec3 rayDir = -ubo.lights[i].position.xyz;
		float tmax = 100000.0f;
		if (ubo.lights[i].type == POINT_LIGHT) {
			rayDir = rayDir * (-1.0f) - fragPos.xyz;
			tmax = length(rayDir);
		}
		rayDir = normalize(rayDir);

        rayQueryInitializeEXT(shadowRayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 
                0xFF, shadowRayOrig, RAY_TMIN, rayDir, tmax);

		bool shadowed = false;
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

		if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
			shadowed = true;
		}
#else
        if (rayQueryProceedEXT(shadowRayQuery))
        {
            shadowed = true;
        }
#endif
        if (!shadowed) {
			float spot = 1.0f;
			if (ubo.lights[i].type == SPOT_LIGHT) {
				spot = CalculateSpotLight(fragPos.xyz, ubo.lights[i]);
			}

			vec3 L = ubo.lights[i].position.xyz;
			vec3 radiance = ubo.lights[i].color;
			if (ubo.lights[i].type == POINT_LIGHT) {
				L -= fragPos.xyz;
				radiance = ApplyAttenuation(ubo.lights[i].color, length(L), ubo.lights[i].radius);
			}
			L = normalize(L);

            float NDF = DistributionGGX(N, normalize(V + L), aoRoughnessMetallic.g);
            float G = GeometrySmith(N, V, L, aoRoughnessMetallic.g);      
            vec3 F = FresnelSchlick(max(dot(normalize(V + L), V), 0.0), F0);
            vec3 numerator = NDF * G * F; 
            float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; 
            vec3 specular = numerator / denominator;
			vec3 kD = vec3(1.0f) - F;
			kD = kD - kD * aoRoughnessMetallic.b;    
            float NdotL = max(dot(N, L), 0.0);        
            outFragcolor.rgb += (kD * albedo.rgb / PI + specular) * radiance * NdotL * spot;
        }
    }

    // Reflection and Refraction Part
	{
		const int iterations = 10;
		float tKr = Kr;
		float tKt = Kt;
		float pKr = Kr;
		float pKt = Kt;
		float bior = 1.0f;
		V = -V;

		vec3 rayOrig = fragPos.xyz;

		if(Kr > 0.0f || Kt > 0.0f) {
			for (int iter = 0; iter < iterations; iter++) {
				//vec3 Fresnel;
				if(tKr > 0.0f) {

					rayQueryEXT rayQuery;
					float tmax = 100000.0f;  

					V = normalize(reflect(V, N));

					rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsNoneEXT, 
					0xFF, rayOrig, RAY_TMIN, V, tmax);

					while (rayQueryProceedEXT(rayQuery)) {
						if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT )  
						{
							rayQueryConfirmIntersectionEXT(rayQuery);
						}
					}

					int geometryID;
					Triangle tri; 
					vec3 aoRoughnessMetallic;
					vec3 reflectionColor = vec3(0.0f);
					if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT )  
					{

						geometryID = rayQueryGetIntersectionGeometryIndexEXT(rayQuery, true);
						int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);

						GeometryNode geometryNode = geometryNodes.nodes[geometryID];
						tri = unpackTriangle(primitiveID, 112, geometryNode.vertexBufferDeviceAddress, geometryNode.indexBufferDeviceAddress, rayQuery);

						if (geometryNode.textureIndexNormal > -1) {
							tri.normal = CalculateNormal(textures[nonuniformEXT(geometryNode.textureIndexNormal)], tri.normal, tri.uv, tri.tangent);
						}

						if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
							tri.color.rgba = pow(texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], tri.uv).rgba, vec4(2.2));
						}

						aoRoughnessMetallic = texture(textures[nonuniformEXT(geometryNode.textureIndexMetallicRoughness)], tri.uv).rgb;
						aoRoughnessMetallic.b *= geometryNode.metallicFactor;
						aoRoughnessMetallic.g *= geometryNode.roughnessFactor;

						reflectionColor += vec3(0.05f) * tri.color.rgb;	// ambient
						reflectionColor += texture(samplerEmissive, inUV).rgb; // emissive

						for(int i = 0; i < numOfLights; ++i)
						{
							rayQueryEXT shadowRayQuery;  

							vec3 shadowRayOrig = tri.pos.xyz + SHADOW_RAY_ORIGIN_MOVEMENT_EPSILON * tri.normal;

							vec3 rayDir = -ubo.lights[i].position.xyz;
							float tmax = 100000.0f;
							if (ubo.lights[i].type == POINT_LIGHT) {
								rayDir = rayDir * (-1.0f) - tri.pos.xyz;
								tmax = length(rayDir);
							}
							rayDir = normalize(rayDir);

							rayQueryInitializeEXT(shadowRayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 
									0xFF, shadowRayOrig, RAY_TMIN, rayDir, tmax);

							bool shadowed = false;
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

							if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
								shadowed = true;
							}
#else
							if (rayQueryProceedEXT(shadowRayQuery))
							{
								shadowed = true;
							}
#endif

							if (!shadowed) {
								float spot = 1.0f;
								if (ubo.lights[i].type == SPOT_LIGHT) {
									spot = CalculateSpotLight(tri.pos, ubo.lights[i]);
								}

								vec3 L = ubo.lights[i].position.xyz;
								vec3 radiance = ubo.lights[i].color;
								if (ubo.lights[i].type == POINT_LIGHT) {
									L -= tri.pos;
									radiance = ApplyAttenuation(ubo.lights[i].color, length(L), ubo.lights[i].radius);
								}
								L = normalize(L);

								vec3 F = FresnelSchlick(max(dot(normalize(V + L), V), 0.0), F0);
								vec3 specular = (DistributionGGX(N, normalize(V + L), aoRoughnessMetallic.g)
													* GeometrySmith(N, V, L, aoRoughnessMetallic.g) * F)
													/ 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;

								vec3 kD = vec3(1.0f) - F;

								kD = kD - kD * aoRoughnessMetallic.b; 
								    
								reflectionColor += (kD * tri.color.rgb / PI + specular) * radiance * max(dot(N, L), 0.0f) * spot;
							}
						}
					}
					else {	// for miss
						outFragcolor.rgb += /*Fresnel */ textureLod(samplerEnvMap, V, 0.0).rgb; 
						break;
					}

					//Fresnel = (F0+(1-F0)*(1-pow(dot(N,V),5)));
					geometryNode = geometryNodes.nodes[geometryID];
					if (geometryNode.reflectance == 0.0f && geometryNode.refractance == 0.0f) {
						outFragcolor.rgb += /*Fresnel */ reflectionColor * pKr;
					}


					N = tri.normal;
					float t = rayQueryGetIntersectionTEXT(rayQuery, true); 
					const vec3 hitPos = rayOrig + t * V;
					rayOrig = hitPos;

					Kr = tKr;
					pKr *= tKr = geometryNode.reflectance;
					ior = geometryNode.ior;
					//F0 = vec3(pow((ior - 1) / (ior + 1), 2));
					F0 = vec3(((ior - 1) / (ior + 1))*((ior - 1) / (ior + 1)));
					F0 = mix(F0, tri.color.rgb, aoRoughnessMetallic.b);
				}
				if(tKt > 0.0f) {
					V = normalize(refract(V, N, bior / ior));

					rayQueryEXT rayQuery;
					float tmax = 100000.0f; 

					rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsNoneEXT, 
					0xFF, rayOrig, RAY_TMIN, V, tmax);

					while (rayQueryProceedEXT(rayQuery)) {
						if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT )  
						{
							rayQueryConfirmIntersectionEXT(rayQuery);
						}
					}

					int geometryID;
					Triangle tri;
					vec3 aoRoughnessMetallic;
					vec3 refractionColor = vec3(0.0f);
					if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT )  
					{
						geometryID = rayQueryGetIntersectionGeometryIndexEXT(rayQuery, true);
						int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);

						GeometryNode geometryNode = geometryNodes.nodes[geometryID];
						tri = unpackTriangle(primitiveID, 112, geometryNode.vertexBufferDeviceAddress, geometryNode.indexBufferDeviceAddress, rayQuery);

						if (geometryNode.textureIndexNormal > -1) {
							tri.normal = CalculateNormal(textures[nonuniformEXT(geometryNode.textureIndexNormal)], tri.normal, tri.uv, tri.tangent);
						}

						if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
							tri.color.rgba = pow(texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], tri.uv).rgba, vec4(2.2));
						}

						aoRoughnessMetallic = texture(textures[nonuniformEXT(geometryNode.textureIndexMetallicRoughness)], tri.uv).rgb;
						aoRoughnessMetallic.b *= geometryNode.metallicFactor;
						aoRoughnessMetallic.g *= geometryNode.roughnessFactor;

						refractionColor += vec3(0.05f) * tri.color.rgb;	// ambient
						refractionColor += texture(samplerEmissive, inUV).rgb; // emissive

						for(int i = 0; i < numOfLights; ++i)
						{
							rayQueryEXT shadowRayQuery;

							vec3 shadowRayOrig = tri.pos.xyz + SHADOW_RAY_ORIGIN_MOVEMENT_EPSILON * tri.normal;

							vec3 rayDir = -ubo.lights[i].position.xyz;
							float tmax = 100000.0f;
							if (ubo.lights[i].type == POINT_LIGHT) {
								rayDir = rayDir * (-1.0f) - tri.pos.xyz;
								tmax = length(rayDir);
							}
							rayDir = normalize(rayDir);

							rayQueryInitializeEXT(shadowRayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 
									0xFF, shadowRayOrig, RAY_TMIN, rayDir, tmax);

							bool shadowed = false;
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

							if (rayQueryGetIntersectionTypeEXT(shadowRayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
								shadowed = true;
							}
#else
							if (rayQueryProceedEXT(shadowRayQuery))
							{
								shadowed = true;
							}
#endif

							if (!shadowed) {
								float spot = 1.0f;
								if (ubo.lights[i].type == SPOT_LIGHT) {
									spot = CalculateSpotLight(tri.pos, ubo.lights[i]);
								}

								vec3 L = ubo.lights[i].position.xyz;
								vec3 radiance = ubo.lights[i].color;
								if (ubo.lights[i].type == POINT_LIGHT) {
									L -= tri.pos;
									radiance = ApplyAttenuation(ubo.lights[i].color, length(L), ubo.lights[i].radius);
								}
								L = normalize(L);

								vec3 F = FresnelSchlick(max(dot(normalize(V + L), V), 0.0f), F0);
								vec3 specular = ( DistributionGGX(N, normalize(V + L), aoRoughnessMetallic.g)
														* GeometrySmith(N, V, L, aoRoughnessMetallic.g) * F)
														/ (4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f);

								vec3 kD = vec3(1.0f) - F;	
	
								kD = kD - kD * aoRoughnessMetallic.b;    

								refractionColor += (kD * tri.color.rgb / PI + specular) * radiance * max(dot(N, L), 0.0f) * spot;
							}
						}
					}
					else {	// for miss
						outFragcolor.rgb += /*(Kr>0.0f?vec3(1.0f)-Fresnel:vec3(1.0f)) */ textureLod(samplerEnvMap, V, 0.0).rgb;
						break;
					}

					geometryNode = geometryNodes.nodes[geometryID];
					if (geometryNode.reflectance == 0.0f && geometryNode.refractance == 0.0f) {
						outFragcolor.rgb += /*(Kr>0.0f?vec3(1.0f)-Fresnel:vec3(1.0f)) */ refractionColor * pKt;
					}

				
					Kt = tKt;
					bior = ior;
					pKr *= tKt = geometryNode.refractance;
					ior = geometryNode.ior;

					N = tri.normal;
					if (dot(V, N) > 0.0f) {	// if the ray is going out from inside to outside.
						N *= -1.0f;
						//ior = 1.5f;
						ior = 1.5f;
					}
					//F0 = vec3(pow((ior - 1) / (ior + 1), 2));
					F0 = vec3(((ior - 1) / (ior + 1))*((ior - 1) / (ior + 1)));
					F0 = mix(F0, tri.color.rgb, aoRoughnessMetallic.b);

					float t = rayQueryGetIntersectionTEXT(rayQuery, true); 
					const vec3 hitPos = rayOrig + t * V;
					rayOrig = hitPos - N * 0.001;
					
				}
				if(iter == iterations - 1 || (tKr == 0.0f && tKt == 0.0f)) {
					break;
				}
				
			}
		}
	}
   

	// Tone Mapping
	outFragcolor.rgb = outFragcolor.rgb / (outFragcolor.rgb + vec3(1.0f));
	// Gamma Correction
	outFragcolor.rgb = pow(outFragcolor.rgb, vec3(1.0f / 2.2f));
}