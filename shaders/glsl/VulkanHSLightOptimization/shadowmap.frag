/*
 * Sogang University
 *
 * Graphics Lab, 2024
 *
 */

#version 460

layout (location = 0) in vec4 fragPos;

layout(binding = 0) uniform UBO
{
    mat4 depthMVP;
} ubo;

//layout (location = 0) out vec4 outLight;

void main()
{
    //gl_FragDepth = fragPos.z;
}