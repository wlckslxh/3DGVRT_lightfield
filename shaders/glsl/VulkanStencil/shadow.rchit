/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Vulkan Full Raytracing
 * 
 */

#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT bool shadowed;

void main()
{
	shadowed = true;
}