/*
 * Sogang University
 *
 * Graphics Lab, 2024-2025
 *
 */

#version 460

layout (location = 0) in vec4 inPos;

layout (binding = 0) uniform UBO
{
	mat4 modelViewProjectionMatrix;
	vec2 gScreenSize;
	vec3 lightPos;
	float lightRadius;
} ubo;

void main() 
{
	gl_Position = ubo.modelViewProjectionMatrix * inPos;
}