/* Copyright (c) 2023, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 * Sogang Univ, Graphics Lab
 */

#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "geometrytypes.glsl"
#include "bufferreferences.glsl"

struct RayPayload {
	vec4 color;
	float distance;
	vec3 normal;
	bool reflector;
	bool refractor;
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec2 attribs;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0) uniform uniformBuffer
{
	mat4 viewInverse;
	mat4 projInverse;
	uint frame;
	vec4 lightPos[2];
} ubo;
layout(binding = 4, set = 0) buffer InstanceInfos { InstanceInfo infos[]; } instanceInfos;

#include "geometryfunctions.glsl"

void main()
{
	Cuboid cuboid = unpackCuboid(gl_InstanceID);

	rayPayload.color = cuboid.color;
	rayPayload.distance = gl_RayTmaxEXT;
	rayPayload.normal = vec3(0.0f);
	rayPayload.reflector = false;
	rayPayload.refractor = false;
}
