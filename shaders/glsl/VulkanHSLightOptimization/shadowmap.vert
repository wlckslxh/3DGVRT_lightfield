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
	mat4 depthMVP;
} ubo;

//out gl_PerVertex 
//{
//    vec4 gl_Position;   
//}; 

layout(location = 0) out vec4 outPos;
 
void main()
{
	gl_Position = ubo.depthMVP * inPos;
}