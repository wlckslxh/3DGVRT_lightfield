/*
 * Abura Soba, 2025
 *
 * 3dgs.glsl
 *
 */

#define NUM_OF_GAUSSIANS 1024 // This macro should be managed with Define.h

#define EPS_T 1e-9
#define MAX_HIT_PER_TRACE 16
#define SPH_MAX_NUM_COEFFS 16	// "configs/render/3dgrt.yaml - particle_radiance_sph_degree" : (x+1) * (x+1)
#define SPECULAR_DIMENSION 45
#define MAX_SPH_DEGREE 3 // "configs/render/3dgrt.yaml - particle_radiance_sph_degree"

#define BUFFER_REFERENCE false	// This macro should be managed with Define.h

#define ENABLE_NORMALS false
#define ENABLE_HIT_COUNTS true
#define PARTICLE_KERNEL_DEGREE 4 // "configs/render/3dgrt.yaml - particle_kernel_degree" : 4
#define SURFEL_PRIMITIVE false // "configs/render/3dgrt.yaml - primitive_type" : instances -> false

#define INVALID_PARTICLE_ID 0xFFFFFFFF
#define INFINITE_DISTANCE 1e20f

#extension GL_EXT_scalar_block_layout : require
//#extension GL_EXT_buffer_reference2 : require

float SH_C0 = 0.28209479177387814f;	// sqrt(1 / (4 * pi))
float SH_C1 = 0.4886025119029199f;	// sqrt(3 / (4 * pi))	3�� ��� ���� ������ float�� �ϳ�
float SH_C2[] = {
	1.0925484305920792f,			// sqrt(15 / (4 * pi))
	-1.0925484305920792f,			// sqrt(15 / (4 * pi))
	0.31539156525252005f,			// sqrt(5 / (16 * pi))
	-1.0925484305920792f,			// sqrt(15 / (4 * pi))
	0.5462742152960396f };			// sqrt(15 / (16 * pi))
float SH_C3[] = {
	-0.5900435899266435f,			// sqrt(35 / (32 * pi))
	2.890611442640554f,				// sqrt(105 / (4 * pi))
	-0.4570457994644658f,			// sqrt(21 / (64 * pi))
	0.3731763325901154f,			// sqrt(7 / (16 * pi))
	-0.4570457994644658f,			// sqrt(105 / (16 * pi))
	1.445305721320277f,				// sqrt(35 / (32 * pi))
	-0.5900435899266435f };			// sqrt(7 / (4 * pi))

struct Aabb {
	float minX, minY, minZ;
	float maxX, maxY, maxZ;
};

struct ParticleDensity {
	vec3 position;
	float density;
	vec4 quaternion;
	vec3 scale;
	float padding;
};

struct Param {
	Aabb aabb;	// TLAS top level aabb

#if BUFFER_REFERENCE
	uint64_t densityBufferDeviceAddress;
	uint64_t sphCoefficientBufferDeviceAddress;
#endif

	float minTransmittance;			// "configs/render/3dgrt.yaml - max_transmittance" : 0.001
	float hitMinGaussianResponse;	// "configs/render/3dgrt.yaml - particle_kernel_min_response" : 0.0113
	float alphaMinThreshold;	// "threedgrt_tracer/optixTracer.cpp" search "alphaMinThreshold" : 1.0f / 255.0f
	uint sphEvalDegree;	// (less or equal than MAX_SPH_DEGREE) "configs/base_gs.yaml - max_n_features" : 3
};

struct RayHit {
	uint particleId;
	float dist;
};

struct RayPayload {
	RayHit hits[MAX_HIT_PER_TRACE];
};

#if BUFFER_REFERENCE
layout(buffer_reference, scalar) buffer Densities { ParticleDensity d[]; };
layout(buffer_reference, scalar) buffer SphCoefficients { float sc[]; };
layout(buffer_reference, scalar) buffer Visibility { float v[]; };
#endif