///*
// * Sogang Univ, Graphics Lab, 2025
// * 
// * Sampled Gaussian Light Field Sphere
// *
// * Closest hit shader
// */

#version 460
#extension GL_EXT_ray_tracing : require
#include "../base/define.glsl"
#include "../base/geometrytypes.glsl"
#include "../base/bufferreferences.glsl"
#include "../base/light.glsl"
#include "../base/pbr.glsl"

layout(set = 1, binding = 0) image2DArray sampledImage;

void main(){

}