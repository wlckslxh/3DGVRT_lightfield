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
    mat4 depthM;
    mat4 depthVP[6];
    vec3 lightPos;
    float farPlane;
} ubo;

void main()
{
    float lightDist = length(fragPos.xyz - ubo.lightPos);

    lightDist = lightDist / ubo.farPlane;

    gl_FragDepth = lightDist;
}