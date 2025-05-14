/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 * 
 * Hybrid (Rasterization + Ray Tracing)
 *
 * Closest hit shader
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


layout(location = 0) rayPayloadInEXT vec3 color;

// Initialized with default value. Appropriate value will be transfered from application.

void main()
{
	color = vec3(1.0f, 0.0f, 0.0f);
}
