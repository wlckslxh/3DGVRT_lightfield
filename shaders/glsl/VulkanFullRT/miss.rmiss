/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 * 
 * Full Ray Tracing
 *
 * Miss shader
 */

#version 460
#extension GL_EXT_ray_tracing : enable

#include "../base/bufferreferences.glsl"

struct RayPayload {
	vec3 color;
	float dist;
	vec3 normal;
	uint effectFlag;
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
layout(binding = 3, set = 0) uniform samplerCube samplerEnvMap;

void main()
{
    vec3 envColor = textureLod(samplerEnvMap, normalize(gl_WorldRayDirectionEXT), 0.0f).rgb;
	rayPayload.color = envColor;
	rayPayload.dist = -1.0f;
	rayPayload.normal = vec3(0.0f);
	rayPayload.effectFlag = 0;
}