/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 * 
 * Hybrid (Rasterization + Ray Tracing)
 *
 * Shadow miss shader
 */

#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT bool shadowed;

void main()
{
	shadowed = false;
}