/*
 * Sogang Univ, Graphics Lab
 */

#version 460
#extension GL_ARB_shading_language_include : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../base/define.glsl"
#include "../base/light.glsl"
#include "../base/pbr.glsl"

// Initialized with default value. Appropriate value will be transfered from application.
layout(constant_id = 0) const uint numOfLights = 1;	
layout(constant_id = 1) const uint numOfDynamicLights = 1;
layout(constant_id = 2) const uint numOfStaticLights = 1;
layout(constant_id = 3) const uint staticLightOffset = 1;

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerMetallicRoughness;
layout (binding = 4) uniform sampler2D samplerEmissive;
layout (binding = 5, set = 0) uniform uniformBuffer
{
	mat4 viewInverse;
	mat4 projInverse;
	Light lights[numOfDynamicLights];
} ubo;
layout (binding = 6, set = 0) uniform uniformBufferStaticLight
{
	Light lights[numOfStaticLights];
} uboStaticLight;
layout (binding = 7) uniform samplerCube samplerEnvMap;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragcolor;

void main() 
{
    vec4 fragPos = texture(samplerPosition, inUV).rgba;
	uint objectID = uint(round(fragPos.w));
    vec3 N = texture(samplerNormal, inUV).rgb;
    vec4 albedo = pow(texture(samplerAlbedo, inUV), vec4(2.2f));

    vec3 aoRoughnessMetallic = texture(samplerMetallicRoughness, inUV).rgb;
    vec3 emissive = texture(samplerEmissive, inUV).rgb;
    if (emissive == vec3(1.0f)) emissive = vec3(0.0f);

	outFragcolor = vec4(0.0f, 0.0f, 0.0f, 1.0f);

	vec2 d = inUV * 2.0f - 1.0f;	// pixel position in NDC
    if(objectID-- == 0) {
		vec4 target = ubo.projInverse * vec4(d.x, d.y, 1.0f, 1.0f) ;    // (pixel position in EC) / Wc
		vec3 direction = normalize(ubo.viewInverse * vec4(normalize(target.xyz), 0.0f)).xyz;
		outFragcolor.rgb = textureLod(samplerEnvMap, direction, 0.0f).rgb;
        return;
	}

    vec3 F0 = mix(vec3(0.04), albedo.rgb, aoRoughnessMetallic.b);
    vec3 V = normalize((ubo.viewInverse * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz - fragPos.xyz);

	outFragcolor.rgb += vec3(0.05f) * albedo.rgb + emissive;

    for(int i = 0; i < numOfLights; ++i) {
		Light light;
		if (i < staticLightOffset)
			light = ubo.lights[i];
		else 
			light = uboStaticLight.lights[i - staticLightOffset];

		vec3 L = light.position.xyz - fragPos.xyz;
		vec3 radiance = ApplyAttenuation(light.color, length(L), light.radius);
		L = normalize(L);

		vec3 F = FresnelSchlick(max(dot(normalize(V + L), V), 0.0f), F0);
		vec3 specular = ( DistributionGGX(N, normalize(V + L), aoRoughnessMetallic.g)
								* GeometrySmith(N, V, L, aoRoughnessMetallic.g) * F)
								/ (4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f);

		vec3 kD = vec3(1.0f) - F;	// kS = F
	
		kD = kD - kD * aoRoughnessMetallic.b;	 
		
        outFragcolor.rgb += (kD * albedo.rgb / PI + specular) * radiance * max(dot(N, L), 0.0f);// * spot;
    }
	// Tone Mapping
	outFragcolor.rgb = outFragcolor.rgb / (outFragcolor.rgb + vec3(1.0f));
	// Gamma Correction
	outFragcolor.rgb = pow(outFragcolor.rgb, vec3(1.0f / 2.2f));
}