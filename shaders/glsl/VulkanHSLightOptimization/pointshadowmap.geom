/*
 * Sogang University
 *
 * Graphics Lab, 2024
 *
 */

#version 460
#extension GL_ARB_shader_viewport_layer_array : enable

layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;

layout(binding = 0) uniform UBO
{
    mat4 depthM;
    mat4 depthVP[6];
    vec3 lightPos;
    float farPlane;
} ubo;

//in gl_PerVertex 
//{
//    vec4 gl_Position;   
//};

layout(location = 0) out vec4 outPos;

void main()
{
    for (int face = 0; face < 6; face++) 
    {
        gl_Layer = face;
        for (int i = 0; i < 3; i++) // for each vertex of triangle
        {
            outPos = gl_in[i].gl_Position;
            gl_Position = ubo.depthVP[face] * outPos;
            EmitVertex();
        }
        EndPrimitive();
    }
}
