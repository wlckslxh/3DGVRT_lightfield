/*
 * Abura Soba, 2025
 *
 * 3dgs.glsl
 *
 */

#define EPS_T 1e-9
#define MAX_HIT_PER_TRACE 16
#define SPH_MAX_NUM_COEFFS 16	// "render/3dgrt.yaml - particle_radiance_sph_degree" : (x+1) * (x+1)

#define BUFFER_REFERENCE false
#define PARTICLE_KERNEL_DEGREE 4 // "render/3dgrt.yaml - particle_kernel_degree" : 4
#define SURFEL_PRIMITIVE false // "render/3dgrt.yaml - primitive_type" : instances -> false

#define INVALID_PARTICLE_ID 0xFFFFFFFF
#define INFINITE_DISTANCE 1e20f

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require

float SH_C0 = 0.28209479177387814f;
float SH_C1 = 0.4886025119029199f;
float SH_C2[] = { 1.0925484305920792f, -1.0925484305920792f, 0.31539156525252005f,
								  -1.0925484305920792f, 0.5462742152960396f };
float SH_C3[] = { -0.5900435899266435f, 2.890611442640554f, -0.4570457994644658f, 0.3731763325901154f,
								  -0.4570457994644658f, 1.445305721320277f, -0.5900435899266435f };

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
	Aabb aabb;

	float minTransmittance;

	uint64_t densityBufferDeviceAddress;
	uint64_t sphCoefficientBufferDeviceAddress;
	float particleRadiance;
	float hitMinGaussianResponse;
	float alphaMinThreshold;
	uint sphDegree;

	float particleVisibility;
};

struct RayHit {
	uint particleId;
	float dist;
};

struct RayPayload {
	RayHit hits[MAX_HIT_PER_TRACE];
};

layout(buffer_reference, scalar) buffer Densities { ParticleDensity d[]; };
layout(buffer_reference, scalar) buffer SphCoefficients { float sc[]; };