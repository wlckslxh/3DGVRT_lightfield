/* Copyright (c) 2023, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 * Sogang Univ, Graphics Lab
 */

const float PI = 3.1415926;
const float DEG_TO_RAD = 0.0174533;

vec3 ApplyAttenuation(vec3 color, float dist, float radius)
{
    if (dist <= radius * pushConstants.lightAtt.alpha) {
        float m = 1.0f / (pushConstants.lightAtt.alpha * radius) * dist; // 1 / (alpha * d) * x
        float n = 1.0f - 1.0f / pushConstants.lightAtt.beta;    // 1 - 1 / beta
        return min(max((1.0f / (m * n * (m - 2.0f) + 1.0f)), 0.001f), 1.0f) * color;
    }
    else {
        float m = pushConstants.lightAtt.alpha * radius;    // alpha * d
        float n = 1.0f / pushConstants.lightAtt.beta;   // 1 / beta
        float i = max(max(color.x, color.y), color.z);
        return min(max((1.0f / ((1.0f / (radius * radius - 2.0f * m * radius + m * m)) * (i / pushConstants.lightAtt.gamma - n) * (dist - m) * (dist - m) + n)), 0.001f), 1.0f) * color;
    }
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    //return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
	return F0 + (1.0 - F0) * clamp(1.0 - cosTheta, 0.0, 1.0) * clamp(1.0 - cosTheta, 0.0, 1.0) * clamp(1.0 - cosTheta, 0.0, 1.0) * clamp(1.0 - cosTheta, 0.0, 1.0) * clamp(1.0 - cosTheta, 0.0, 1.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

vec3 CalculateNormal(sampler2D normalMap, vec3 normal, vec2 tex_coord, vec4 tangent)
{
    vec3 tangentNormal = texture(normalMap, tex_coord).xyz * 2.0 - 1.0;

    vec3 N = normalize(normal);
    vec3 T = normalize(tangent.xyz);
    vec3 B = tangent.w * normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}