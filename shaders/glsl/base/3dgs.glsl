/*
 * Abura Soba, 2025
 *
 * 3dgs.glsl
 *
 */

#define EPS_T 1e-9
#define MAX_HIT_PER_TRACE 16
#define INVALID_PARTICLE_ID 0xFFFFFFFF
#define INFINITE_DISTANCE 1e20f

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

	ParticleDensity particleDensity;
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