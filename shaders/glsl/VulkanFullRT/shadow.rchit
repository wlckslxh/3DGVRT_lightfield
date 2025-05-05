/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 * 
 * Full Ray Tracing
 *
 * Shadow hit shader
 */

#version 460
#extension GL_EXT_ray_tracing : require

#include "../base/bufferreferences.glsl"

layout(location = 2) rayPayloadInEXT bool shadowed;

void main()
{
	shadowed = true;
}