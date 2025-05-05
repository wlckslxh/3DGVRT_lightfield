/* Copyright (c) 2023, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 * Sogang Univ, Graphics Lab
 */

struct Vertex
{
	vec3 pos;
	vec3 normal;
	vec2 uv;
	vec4 color;
	vec4 tangent;
};

struct Triangle {
	Vertex vertices[3];
	vec3 pos;
	vec3 normal;
	vec2 uv;
	vec4 color;
	vec4 tangent;
};

struct GeometryNode {
	uint64_t vertexBufferDeviceAddress;
	uint64_t indexBufferDeviceAddress;
	int textureIndexBaseColor;
	int textureIndexOcclusion;
	int textureIndexNormal;
	int textureIndexMetallicRoughness;
	int textureIndexEmissive;
	float reflectance;
	float refractance;
	float ior;
	float metallicFactor;
	float roughnessFactor;
};

struct InstanceInfo {
	uint64_t geometryNodeBufferDeviceStartAddress;
	mat4 transformMatrix;
	mat4 transformInverseTrans;
};