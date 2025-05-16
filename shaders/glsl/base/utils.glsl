/*
 * Abura Soba, 2025
 *
 * utils.glsl
 *
 */

vec3 safe_normalize(vec3 v) {
    const float l = v.x * v.x + v.y * v.y + v.z * v.z;
    return l > 0.0f ? (v * inversesqrt(l)) : v;
}

mat3 quaternionWXYZToMatrix(const vec4 q) {
    const float w = q.x;
    const float x = q.y;
    const float y = q.z;
    const float z = q.w;

    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    return mat3(
        1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy),
        2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx),
        2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy)
    );
}

vec2 intersectAABB(const Aabb aabb, vec3 rayOri, vec3 rayDir) {
    vec3 t0 = (vec3(aabb.minX, aabb.minY, aabb.minZ) - rayOri) / max(rayDir, vec3(1e-6));
    vec3 t1 = (vec3(aabb.maxX, aabb.maxY, aabb.maxZ) - rayOri) / max(rayDir, vec3(1e-6));
    vec3 tmax = vec3(max(t0.x, t1.x), max(t0.y, t1.y), max(t0.z, t1.z));
    vec3 tmin = vec3(min(t0.x, t1.x), min(t0.y, t1.y), min(t0.z, t1.z));
    float maxOfMin = max(0.0f, max(tmin.x, max(tmin.y, tmin.z)));
    float minOfMax = min(tmax.x, min(tmax.y, tmax.z));
    return vec2(maxOfMin, minOfMax);
}