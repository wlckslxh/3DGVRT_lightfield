/*
 * Sogang University
 *
 * Graphics Lab, 2024
 *
 */

#version 460

#include "../base/light.glsl"

layout (location = 0) in vec4 inPos;

layout(constant_id = 0) const uint numOfLights = 100;	
layout(constant_id = 1) const uint numOfPointLights = 1;

layout (binding = 5) uniform UBO 
{
	mat4 modelViewProjectionMatrix;
	mat4 viewInverse;
	mat4 projInverse;
    Light lights[numOfLights];
} ubo;

layout (location = 0) out vec2 outUV;

void main() 
{
	gl_Position = ubo.modelViewProjectionMatrix * inPos;
}
