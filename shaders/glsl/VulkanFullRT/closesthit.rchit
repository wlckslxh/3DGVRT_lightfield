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
	switch (gl_PrimitiveID % 20) {
		case 0:  color = vec3(1.0f, 0.0f, 0.0f); break;       // red
		case 1:  color = vec3(0.0f, 1.0f, 0.0f); break;       // green
		case 2:  color = vec3(0.0f, 0.0f, 1.0f); break;       // blue
		case 3:  color = vec3(1.0f, 1.0f, 0.0f); break;       // yellow
		case 4:  color = vec3(0.0f, 1.0f, 1.0f); break;       // cyan
		case 5:  color = vec3(1.0f, 0.0f, 1.0f); break;       // magenta
		case 6:  color = vec3(1.0f, 0.5f, 0.0f); break;       // orange
		case 7:  color = vec3(0.5f, 0.0f, 1.0f); break;       // violet
		case 8:  color = vec3(0.5f, 1.0f, 0.0f); break;       // lime green
		case 9:  color = vec3(0.0f, 0.5f, 1.0f); break;       // sky blue
		case 10: color = vec3(1.0f, 0.0f, 0.5f); break;       // pink-red
		case 11: color = vec3(0.5f, 1.0f, 1.0f); break;       // light cyan
		case 12: color = vec3(0.7f, 0.7f, 0.7f); break;       // gray
		case 13: color = vec3(0.3f, 0.3f, 0.3f); break;       // dark gray
		case 14: color = vec3(0.9f, 0.4f, 0.1f); break;       // brownish
		case 15: color = vec3(0.6f, 0.2f, 0.8f); break;       // purple
		case 16: color = vec3(0.2f, 0.6f, 0.8f); break;       // teal
		case 17: color = vec3(0.8f, 0.8f, 0.2f); break;       // pale yellow
		case 18: color = vec3(0.3f, 0.7f, 0.4f); break;       // grass green
		case 19: color = vec3(0.8f, 0.2f, 0.2f); break;       // dark red
	}
}
