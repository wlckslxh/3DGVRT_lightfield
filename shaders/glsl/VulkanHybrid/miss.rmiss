/* Copyright (c) 2023, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 * Sogang Univ, Graphics Lab
 */

#version 460
#extension GL_EXT_ray_tracing : enable

struct RayPayload {
	vec3 color;
	float dist;
	vec3 normal;
	uint objectID;
	vec3 albedo;
	float metallic;
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
layout(binding = 8) uniform samplerCube samplerEnvMap;

void main()
{
	vec3 worldRayDir = normalize(gl_WorldRayDirectionEXT);
    vec3 envColor = textureLod(samplerEnvMap, worldRayDir, 0.0).rgb;
	rayPayload.color = envColor;
	rayPayload.dist = -1.0f;
	rayPayload.normal = vec3(0.0f);
	rayPayload.albedo = vec3(0.0f);
	rayPayload.metallic = 0.0f;
}