/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 *
 * define.glsl
 *
 */

#define RAY_TMIN 0.1f
#define SHADOW_RAY_ORIGIN_MOVEMENT_EPSILON 0.1f	

#define ANY_HIT 0	// This macro should be managed with Define.h

#define ITERATIONS 5

struct RayOption {
	bool shadowRay;
	bool reflection;
	bool refraction;
};

struct LightAttVar {
	float alpha;
	float beta;
	float gamma;
};

layout(push_constant, std430) uniform PushConstants {
	RayOption rayOption;
	LightAttVar lightAtt;
} pushConstants;