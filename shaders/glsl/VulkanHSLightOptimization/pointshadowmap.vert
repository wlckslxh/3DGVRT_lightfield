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
	mat4 depthM;
	mat4 depthVP[6];
	vec3 lightPos;
    float farPlane;
} ubo;

//out gl_PerVertex 
//{
//    vec4 gl_Position;   
//}; 

 
void main()
{
	gl_Position = ubo.depthM * inPos;
}