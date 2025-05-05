/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Vulkan Full Raytracing
 * 
 */

#version 460
#extension GL_EXT_ray_tracing : require

#include "../base/bufferreferences.glsl"

layout(location = 2) rayPayloadInEXT bool shadowed;

void main()
{
	shadowed = true;
}