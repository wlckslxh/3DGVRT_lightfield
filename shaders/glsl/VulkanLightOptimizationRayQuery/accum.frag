/*
 * Sogang University
 *
 * Graphics Lab, 2024-2025
 *
 */

#version 460

layout (binding = 0) uniform UBO
{
	vec3 lightPos;
	float lightRadius;
} ubo;
layout (binding = 1) uniform sampler2D samplerPosition;

layout (location = 0) in vec2 inUV;

layout (location = 0) out uint outAccum;

layout (push_constant) uniform PushConst 
{
	uint lightIdx;
} pushConst;

void main() 
{
	vec4 fragPos = texture(samplerPosition, inUV).rgba;
	float dist = length(fragPos.xyz - ubo.lightPos);

	if (dist < ubo.lightRadius) {
		outAccum = 1u << pushConst.lightIdx;
	}
	else {
		outAccum = 0u;
	}
}