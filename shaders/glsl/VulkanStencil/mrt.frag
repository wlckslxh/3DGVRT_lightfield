/*
 * Sogang University
 *
 * Graphics Lab, 2024
 *
 */

#version 460

layout (binding = 0) uniform sampler2D samplerEnvMap;
layout (binding = 1) uniform sampler2D samplerMetallicRoughnessMap;
layout (binding = 2) uniform sampler2D samplerNormalMap;
layout (binding = 3) uniform sampler2D samplerEmissiveMap;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec4 inWorldPos;
layout (location = 3) in vec4 inTangent;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;
layout (location = 3) out vec4 outMetallicRoughness;
layout (location = 4) out vec4 outEmissive;

vec3 CalculateNormal(sampler2D normalMap, vec3 normal, vec2 tex_coord, vec4 tangent)
{
    vec4 normalVal = texture(normalMap, tex_coord);

	//for objects not using normal texture
    if (normalVal.a == 0.0f)
        return normalize(inNormal);
    
    vec3 tangentNormal = normalVal.xyz * 2.0 - 1.0;

    vec3 N = normalize(normal);
    vec3 T = normalize(tangent.xyz);
    vec3 B = tangent.w * normalize(cross(N, T));	//tangent.w : handness of tangent basis
													//			  +1: right-handed
													//			  -1: left-handed
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

void main() 
{
	outAlbedo = texture(samplerEnvMap, inUV);
	if (outAlbedo.a == 0.0f)
	{
		discard;
		return;
	}

	outPosition = inWorldPos;

	// Calculate normal in tangent space
	vec3 tnorm = CalculateNormal(samplerNormalMap, normalize(inNormal), inUV, inTangent);
	outNormal = vec4(tnorm, 1.0);

	outMetallicRoughness = texture(samplerMetallicRoughnessMap, inUV);
	outEmissive = texture(samplerEmissiveMap, inUV);

}