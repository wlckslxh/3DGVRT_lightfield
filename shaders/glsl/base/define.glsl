/*
 * Sogang University
 *
 * Graphics Lab, 2024
 *
 */

#define DO_NORMAL_MAPPING

#define SPLIT_BLAS 1	// This macro should be managed with Define.h

#define RAY_TMIN 0.1f
#define SHADOW_RAY_ORIGIN_MOVEMENT_EPSILON 0.1f	

//#define USE_RAY_QUERY
#define ANY_HIT 0	// This macro should be managed with Define.h

#define ITERATIONS 5

#define DYNAMIC_SCENE 0 // This macro should be managed with Define.h

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