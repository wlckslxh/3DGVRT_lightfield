/*
 * Abura Soba, 2025
 *
 * gaussianfunctions.glsl
 *
 */

vec2 intersectAABB(const Aabb aabb, vec3 rayOri, vec3 rayDir) {
    vec3 t0 = (vec3(aabb.minX, aabb.minY, aabb.minZ) - rayOri) / max(rayDir, vec3(1e-6));
    vec3 t1 = (vec3(aabb.maxX, aabb.maxY, aabb.maxZ) - rayOri) / max(rayDir, vec3(1e-6));
    vec3 tmax = vec3(max(t0.x, t1.x), max(t0.y, t1.y), max(t0.z, t1.z));
    vec3 tmin = vec3(min(t0.x, t1.x), min(t0.y, t1.y), min(t0.z, t1.z));
    float maxOfMin = max(0.0f, max(tmin.x, max(tmin.y, tmin.z)));
    float minOfMax = min(tmax.x, min(tmax.y, tmax.z));
    return vec2(maxOfMin, minOfMax);
}

float particleResponse(float grayDist) {
    switch (PARTICLE_KERNEL_DEGREE) {
    case 8: // Zenzizenzizenzic
    {
        float s = -0.000685871056241f;
        const float grayDistSq = grayDist * grayDist;
        return exp(s * grayDistSq * grayDistSq);
    }
    case 5: // Quintic
    {
        float s = -0.0185185185185f;
        return exp(s * grayDist * grayDist * sqrt(grayDist));
    }
    case 4: // Tesseractic
    {
        float s = -0.0555555555556f;
        return exp(s * grayDist * grayDist);
    }
    case 3: // Cubic
    {
        float s = -0.166666666667f;
        return exp(s * grayDist * sqrt(grayDist));
    }
    case 1: // Laplacian
    {
        float s = -1.5f;
        return exp(s * sqrt(grayDist));
    }
    case 0: // Linear
    {
        /* static const */ float s = -0.329630334487f;
        return max(1.f + s * sqrt(grayDist), 0.f);
    }
    default: // Quadratic
    {
        float s = -0.5f;
        return exp(s * grayDist);
    }
    }
}

#if BUFFER_REFERENCE
void fetchParticleDensity(
    const uint particleIdx,
    const uint64_t densityBufferDeviceAddress,
    out vec3 particlePosition,
    out vec3 particleScale,
    out mat3 particleRotation,
    out float particleDensity) {
    const ParticleDensity particleData = Densities[nonuniformEXT(densityBufferDeviceAddress)].d[nonuniformEXT(particleIdx)];

    particlePosition = particleData.position;
    particleScale = particleData.scale;
    particleRotation = quaternionWXYZToMatrix(particleData.quaternion);
    particleDensity = particleData.density;
}

void fetchParticleSphCoefficients(
    const uint particleIdx,
    //const float sphCoefficientBufferDeviceAddress,
    out vec3 sphCoefficients[]) {
    const uint particleOffset = particleIdx * SPH_MAX_NUM_COEFFS * 3;
    for (unsigned int i = 0; i < SPH_MAX_NUM_COEFFS; i++) {
        const int offset = i * 3;
        sphCoefficients[i] = vec3(
            particlesSphCoefficients[nonuniformEXT(particleOffset + offset + 0)],
            particlesSphCoefficients[nonuniformEXT(particleOffset + offset + 1)],
            particlesSphCoefficients[nonuniformEXT(particleOffset + offset + 2]);
    }
}
#else
void fetchParticleDensity(
    const uint particleIdx,
    out vec3 particlePosition,
    out vec3 particleScale,
    out mat3 particleRotation,
    out float particleDensity) {
    const ParticleDensity particleData = particleDensities.d[nonuniformEXT(particleIdx)];

    particlePosition = particleData.position;
    particleScale = particleData.scale;
    particleRotation = quaternionWXYZToMatrix(particleData.quaternion);
    particleDensity = particleData.density;
}

// load spherical harmonics coefficient
void fetchParticleSphCoefficients(
    const uint particleIdx,
    out vec3 sphCoefficients[SPH_MAX_NUM_COEFFS]) {
    const uint particleOffset = particleIdx * SPH_MAX_NUM_COEFFS * 3;	// each has 3 elements
    for (uint i = 0; i < SPH_MAX_NUM_COEFFS; i++) {
        uint offset = i * 3;	// each has 3 elements
        sphCoefficients[i] = vec3(
            particleSphCoefficients.c[nonuniformEXT(particleOffset + offset + 0)],
            particleSphCoefficients.c[nonuniformEXT(particleOffset + offset + 1)],
            particleSphCoefficients.c[nonuniformEXT(particleOffset + offset + 2)]);
    }
}
#endif

// calc spherical harmonics with coefficients
// not a special algorithm
// algorithm from spherical harmonics
// "parametric radiance function" in paper
vec3 radianceFromSpH(uint deg, const vec3 sphCoefficients[SPH_MAX_NUM_COEFFS], const vec3 rdir, bool clamped) {
    vec3 rad = SH_C0 * sphCoefficients[0];
    if (deg > 0) {
        const vec3 dir = rdir;

        const float x = dir.x;
        const float y = dir.y;
        const float z = dir.z;
        rad = rad - SH_C1 * y * sphCoefficients[1] + SH_C1 * z * sphCoefficients[2] - SH_C1 * x * sphCoefficients[3];

        if (deg > 1) {
            const float xx = x * x, yy = y * y, zz = z * z;
            const float xy = x * y, yz = y * z, xz = x * z;
            rad = rad + SH_C2[0] * xy * sphCoefficients[4] + SH_C2[1] * yz * sphCoefficients[5] +
                SH_C2[2] * (2.0f * zz - xx - yy) * sphCoefficients[6] +
                SH_C2[3] * xz * sphCoefficients[7] + SH_C2[4] * (xx - yy) * sphCoefficients[8];
            if (deg > 2) {
                rad = rad + SH_C3[0] * y * (3.0f * xx - yy) * sphCoefficients[9] +
                    SH_C3[1] * xy * z * sphCoefficients[10] +
                    SH_C3[2] * y * (4.0f * zz - xx - yy) * sphCoefficients[11] +
                    SH_C3[3] * z * (2.0f * zz - 3.0f * xx - 3.0f * yy) * sphCoefficients[12] +
                    SH_C3[4] * x * (4.0f * zz - xx - yy) * sphCoefficients[13] +
                    SH_C3[5] * z * (xx - yy) * sphCoefficients[14] +
                    SH_C3[6] * x * (xx - 3.0f * yy) * sphCoefficients[15];
            }
        }
    }
    rad += 0.5f;
    return clamped ? max(rad, vec3(0.0f)) : rad;
}

bool processHit(
	vec3 rayOrigin,
	vec3 rayDirection,
	uint particleIdx,
#if BUFFER_REFERENCE
	const uint64_t densityBufferDeviceAddress,
	const uint64_t sphCoefficientBufferDeviceAddress,
#endif
	float minParticleKernelDensity,
	float minParticleAlpha,
	uint sphEvalDegree,
	inout float transmittance,
	inout vec4 radiance,
	inout float depth
#if ENABLE_NORMALS
	,inout vec3 normal
#endif
	){
	vec3 particlePosition;
	vec3 particleScale;
	mat3 particleRotation;
	float particleDensity;
	
	fetchParticleDensity(
        particleIdx,
#if BUFFER_REFERENCE
        densityBufferDeviceAddress,
#endif
        particlePosition,
        particleScale,
        particleRotation,
        particleDensity);

	const vec3 giscl   = vec3(1 / particleScale.x, 1 / particleScale.y, 1 / particleScale.z);
    const vec3 gposc   = (rayOrigin - particlePosition);
    const vec3 gposcr  = (particleRotation * gposc);
    const vec3 gro     = giscl * gposcr;
    const vec3 rayDirR = particleRotation * rayDirection;
    const vec3 grdu    = giscl * rayDirR;
    const vec3 grd     = safeNormalize(grdu);

	const vec3 gcrod = SURFEL_PRIMITIVE ? gro + grd * -gro.z / grd.z : cross(grd, gro);
	const float grayDist = dot(gcrod, gcrod);

	const float gres = particleResponse(grayDist);
	const float galpha = min(0.99f, gres * particleDensity);

	const bool acceptHit = (gres > minParticleKernelDensity) && (galpha > minParticleAlpha);

	if (acceptHit) {
        const float weight = galpha * (transmittance);

		const vec3 grds = particleScale * grd * (SURFEL_PRIMITIVE ? -gro.z / grd.z : dot(grd, -1 * gro));
		const float hitT = sqrt(dot(grds, grds));

		vec3 sphCoefficients[SPH_MAX_NUM_COEFFS];
		fetchParticleSphCoefficients(
			particleIdx,
//#if BUFFER_REFERENCE
//			sphCoefficientBufferDeviceAddress,
//#endif
			sphCoefficients);
		const vec3 grad = radianceFromSpH(sphEvalDegree, sphCoefficients, rayDirection, true);

		radiance += vec4(grad * weight, 1.0f);
		transmittance *= (1 - galpha);
		depth += hitT * weight;

//#if ENABLE_NORMALS
//		const float ellispoidSqRadius = 9.0f;
//		const vec3 particleScaleRotated = (particleRotation * particleScale);
//		normal += weight * (SURFEL_PRIMITIVE ? vec3(0, 0, (grd.z > 0 ? 1 : -1) * particleScaleRotated.z) : safeNormalize((gro + grd * (dot(grd, -1 * gro) - sqrt(ellispoidSqRadius - grayDist))) * particleScaleRotated));
//#endif
	}

	return acceptHit;
}