/*
 * Sogang University
 *
 * Graphics Lab, 2024
 *
 */

#version 460

layout (location = 0) in vec4 inPos;

layout (binding = 0) uniform UBO 
{
	mat4 modelViewProjectionMatrix;
} ubo;

void main()
{
	gl_Position = ubo.modelViewProjectionMatrix * inPos;
}