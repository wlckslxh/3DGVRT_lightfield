/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Vulkan Hybrid Shadow map Optimization for Light calculation
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
	float distance;
	vec3 normal;
	uint objectID;
};
layout(constant_id = 0) const uint numOfLights = 1;	
layout(constant_id = 1) const uint numOfPointLights = 1;

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec2 attribs;

layout(binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;
layout(binding = 3) uniform uniformBuffer
{
	mat4 viewInverse;
	mat4 projInverse;
	mat4 lightSpace[numOfLights - numOfPointLights];
	Light lights[numOfLights];
} ubo;
layout(binding = 10) uniform sampler2D shadowMap[numOfLights - numOfPointLights];
layout(set = 0, binding = 11) uniform samplerCube pointShadowMap[numOfPointLights];
layout(binding = 12) uniform sampler2D textures[];

#include "../base/geometryfunctions.glsl"

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

#define amb 0.1

float textureProj(vec4 shadowCoord, vec2 off, uint shadowmapIdx)
{
    float shadow = 1.0;

    if (shadowCoord.z > -1.0f && shadowCoord.z < 1.0f) 
    {
        float dist = texture(shadowMap[shadowmapIdx], shadowCoord.st + off).r;
		if (shadowCoord.w > 0.0f && dist < shadowCoord.z)
        {
            shadow = amb;
        }
    }
    return shadow;
}

float filterPCF(vec4 sc, uint shadowmapIdx)
{
    ivec2 texDim = textureSize(shadowMap[shadowmapIdx], 0);
    float scale = 1.5f;
    float dx = scale * 1.0f / float(texDim.x);
    float dy = scale * 1.0f / float(texDim.y);

    float shadowFactor = 0.0f;
    int count = 0;
    int range = 2;  // PCF ���͸� ������ �������� �ε巯�� �׸��� ó��

    for (int x = -range; x <= range; x++)
    {
        for (int y = -range; y <= range; y++)
        {
            shadowFactor += textureProj(sc, vec2(dx * x, dy * y), shadowmapIdx);
            count++;
        }
    }
    return shadowFactor / count;
}

float CalculateShadow(vec3 shadowVector, uint shadowmapIdx)
{
	float curDepth = length(shadowVector);
	shadowVector = normalize(shadowVector);

	float sampledDepth = texture(pointShadowMap[nonuniformEXT(shadowmapIdx)], shadowVector).r;
	sampledDepth *= ubo.farPlane;

	float ret = 1.0f;
	if (sampledDepth < curDepth - 0.1f)
	{
		ret = amb;
	}

	return ret;
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
	GeometryNode geometryNode = geometryNodes.nodes[nonuniformEXT(gl_GeometryIndexEXT)];

	Triangle tri = unpackTriangle(gl_PrimitiveID, 112, geometryNode.vertexBufferDeviceAddress, geometryNode.indexBufferDeviceAddress);

	#ifdef DO_NORMAL_MAPPING
	if (geometryNode.textureIndexNormal > -1) {
		tri.normal = CalculateNormal(textures[nonuniformEXT(geometryNode.textureIndexNormal)], tri.normal, tri.uv, tri.tangent);
	}
	#endif

	if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
		tri.color.rgb = pow(texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], tri.uv).rgb, vec3(2.2));
	}

	vec3 aoRoughnessMetallic = texture(textures[nonuniformEXT(geometryNode.textureIndexMetallicRoughness)], tri.uv).rgb;

	vec3 V = normalize((ubo.viewInverse * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz - tri.pos.xyz);

	rayPayload.color = vec3(0.05f) * tri.color.rgb; // ambient

	rayPayload.distance = gl_RayTmaxEXT;
	rayPayload.normal = tri.normal;
	rayPayload.objectID = gl_GeometryIndexEXT;
	
	float ior = geometryNode.ior;
	//vec3 F0 = vec3(pow((ior - 1) / (ior + 1), 2));
	vec3 F0 = vec3(((ior - 1) / (ior + 1))*((ior - 1) / (ior + 1)));
	F0 = mix(F0, tri.color.rgb, aoRoughnessMetallic.b);

	vec4 ShadowCoord;
	float shadow;
	for(int i = 0; i < numOfLights; i++) {
		shadowed = false;
		if (pushConstants.rayOption.shadowRay) {
			if (ubo.lights[i].type != POINT_LIGHT) {
				ShadowCoord = ( biasMat * ubo.lightSpace[i] ) * vec4(tri.pos.xyz, 1.0f);
	//			shadow = textureProj(ShadowCoord / ShadowCoord.w, vec2(0.0));	// No PCF version.
				shadow = filterPCF(ShadowCoord / ShadowCoord.w, i);
			}
			else {
				shadow = CalculateShadow(tri.pos.xyz - ubo.lights[i].position.xyz, i - (numOfLights - numOfPointLights));
			}
			shadowed = (shadow != 1.0f);
		}

		if (!shadowed) {
			vec3 L = ubo.lights[i].position.xyz - tri.pos.xyz;
			vec3 radiance = ApplyAttenuation(ubo.lights[i].color, length(L), ubo.lights[i].radius);
			L = normalize(L);
    
			vec3 F    = FresnelSchlick(max(dot(normalize(V + L), V), 0.0f), F0);
			vec3 specular = ( DistributionGGX(tri.normal, normalize(V + L), aoRoughnessMetallic.g)
									* GeometrySmith(tri.normal, V, L, aoRoughnessMetallic.g) * F )
									/ (4.0f * max(dot(tri.normal, V), 0.0f) * max(dot(tri.normal, L), 0.0f) + 0.0001f);

			vec3 kD = vec3(1.0f) - F;

			kD = kD - kD * aoRoughnessMetallic.b;	      

			rayPayload.color += (kD * tri.color.rgb / PI + specular) * radiance * max(dot(tri.normal, L), 0.0f) * spot;
		}
    }
}
