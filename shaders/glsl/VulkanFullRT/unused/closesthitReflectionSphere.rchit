///* Copyright (c) 2023, Sascha Willems
// *
// * SPDX-License-Identifier: MIT
// *
// * Sogang Univ, Graphics Lab
// */
//
//#version 460
//
//#extension GL_EXT_ray_tracing : require
//#extension GL_GOOGLE_include_directive : require
//#extension GL_EXT_nonuniform_qualifier : require
//#extension GL_EXT_buffer_reference2 : require
//#extension GL_EXT_scalar_block_layout : require
//#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
//
//#include "geometrytypes.glsl"
//#include "bufferreferences.glsl"
//#include "../base/light.glsl"
//
//struct RayPayload {
//	vec3 color;
//	float distance;
//	vec3 normal;
//	uint effectFlag;
//};
//
//layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
//layout(location = 2) rayPayloadEXT bool shadowed;
//hitAttributeEXT vec2 attribs;
//
//layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
//layout(binding = 2, set = 0) uniform uniformBuffer
//{
//	mat4 viewInverse;
//	mat4 projInverse;
//	uint frame;
//	vec4 lightPos[2];
//} ubo;
//layout(binding = 4, set = 0) buffer InstanceInfos { InstanceInfo infos[]; } instanceInfos;
//
//#include "geometryfunctions.glsl"
//
//void main()
//{
//	vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;	// hit position at sphere
//
//	Sphere sphere = unpackSphere(gl_InstanceID);
//
//	rayPayload.color = sphere.color.rgb;
//	rayPayload.distance = gl_RayTmaxEXT;
//	rayPayload.normal = normalize(worldPos - sphere.center);
//	rayPayload.effectFlag = 0x1;
//
//	// Shadow casting
//	float tmin = 0.001f;
//	float tmax = 10000.0f;
//	float epsilon = 0.001f;
//	vec3 origin = worldPos;// + rayPayload.normal * epsilon;
//
//	shadowed = true;  
//
//	// Trace shadow ray and offset indices to match shadow hit/miss shader group indices
//	traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 0, 0, 1, origin, tmin, normalize(ubo.lightPos[0].xyz - origin), tmax, 2);
//	if (shadowed) {
//		rayPayload.color *= 0.7;
//	}
//}
//