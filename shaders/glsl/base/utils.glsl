/*
 * Abura Soba, 2025
 *
 * utils.glsl
 *
 */

vec3 safeNormalize(vec3 v) {
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

mat3 quaternionWXYZToMatrixTranspose(const vec4 q) {
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
        1.0 - 2.0 * (yy + zz), 2.0 * (xy + wz), 2.0 * (xz - wy),
		2.0 * (xy - wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz + wx),
		2.0 * (xz + wy), 2.0 * (yz - wx), 1.0 - 2.0 * (xx + yy)
    );
}