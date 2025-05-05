/*
 * Sogang University
 *
 * Graphics Lab, 2024
 *
 */

#version 460

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec4 inTangent;
layout (location = 4) in uint inObjectID;

layout (binding = 4) uniform UBO 
{
	mat4 modelMatrix;
	mat4 modelViewProjectionMatrix;
	mat4 modelMatrixInvTrans;
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec4 outWorldPos;
layout (location = 3) out vec4 outTangent;

void main() 
{
	gl_Position = ubo.modelViewProjectionMatrix * inPos;
	
	outUV = inUV;

	outWorldPos = vec4(vec3(ubo.modelMatrix * inPos), inObjectID + 1);
	
	// Normal in world space
	outNormal = (ubo.modelMatrixInvTrans * vec4(normalize(inNormal), 0.0f)).xyz;	
	outTangent = vec4((ubo.modelMatrixInvTrans * vec4(normalize(inTangent.xyz), 0.0f)).xyz, inTangent.w);
}