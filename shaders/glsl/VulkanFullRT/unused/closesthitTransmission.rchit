/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Vulkan Full Raytracing
 * 
 */

#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "geometrytypes.glsl"
#include "bufferreferences.glsl"
#include "../base/light.glsl"
#include "../base/pbr.glsl"

struct RayPayload {
	vec3 color;
	float distance;
	vec3 normal;
	uint effectFlag;
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec2 attribs;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0) uniform uniformBuffer
{
	mat4 viewInverse;
	mat4 projInverse;
	uint frame;
	vec4 lightPos[1];
} ubo;
layout(binding = 3, set = 0) uniform sampler2D image;
layout(binding = 4, set = 0) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;
layout(binding = 6, set = 0) uniform sampler2D textures[];

#include "geometryfunctions.glsl"

void main()
{
	Triangle tri = unpackTriangle(gl_PrimitiveID, 112);
	GeometryNode geometryNode = geometryNodes.nodes[nonuniformEXT(gl_GeometryIndexEXT)];

	vec3 pos = tri.pos.xyz;

	vec3 albedo = tri.color.rgb;
	if (geometryNode.textureIndexBaseColor > -1) {
		albedo = pow(texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], tri.uv).rgb, vec3(2.2f));
	}

	vec2 metallicRoughness = texture(textures[nonuniformEXT(geometryNode.textureIndexMetallicRoughness)], tri.uv).gb;
	float metallic = metallicRoughness.y;
	float roughness = metallicRoughness.x;

	vec3 N = tri.normal;
	if (geometryNode.textureIndexNormal > -1) {
		N = CalculateNormal(textures[nonuniformEXT(geometryNode.textureIndexNormal)], tri.normal, tri.uv, tri.tangent);
	}

	vec3 V = normalize((ubo.viewInverse * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz - pos);

	float Kr = geometryNode.reflectance;
	float Kt = geometryNode.refractance;
	if(Kr>0.0f || Kt>0.0f){
		albedo=vec3(0.0f);
	}

	float ior = geometryNode.ior;
	vec3 F0 = vec3(pow((ior - 1) / (ior + 1), 2));
	F0 = mix(F0, albedo, metallic);

	vec3 Lo = vec3(0.0f);

	int numOfLights = NUM_OF_LIGHTS;
	bool shadowRayOn = SHADOW_RAY_ON;
	for(int i = 0; i < numOfLights; i++) {

		// Shadow casting
		float tmin = 0.1f;
		float epsilon = 0.1f;
		vec3 rayDirection = normalize(ubo.lightPos[i].xyz - pos);	// current hit spot to light

		vec3 origin = pos + rayDirection * epsilon;

		float tmax = length(ubo.lightPos[i].xyz - origin);

		shadowed = false;
		if (shadowRayOn) {
			shadowed = true;  
			traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 0, 0, 1, origin, tmin, rayDirection, tmax, 2);
		}
		if (!shadowed) {
			// Spot light unused
			/*vec3 spot = vec3(1.0f);
			if (lightInfo[i].spotCutoff != 180.0f) {
				spot = CalculateSpotLight(pos, i, lightInfo[i], ubo.lightPos[i]);
			}*/

			vec3 L = normalize(ubo.lightPos[i].xyz - pos);

			vec3 H = normalize(V + L);

			vec3 radiance = lightInfo[i].color;
			// Light attenuation unused
			/*float dist = length(ubo.lightPos[i].xyz - pos); 
			radiance = ApplyAttenuation(lightInfo[i].color, dist);*/

			float NDF = DistributionGGX(N, H, roughness);
			float G   = GeometrySmith(N, V, L, roughness);      
			vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);
           
			vec3 numerator    = NDF * G * F; 
			float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; 
			vec3 specular = numerator / denominator;
        
			vec3 kS = F;

			vec3 kD = vec3(1.0) - kS;

			kD *= 1.0 - metallic;	  

			float NdotL = max(dot(N, L), 0.0);        

			Lo += (kD * albedo / PI + specular) * radiance * NdotL;// * spot;
		}
    }

	vec3 ambient = vec3(0.05) * albedo;

	vec3 pbrColor = ambient + Lo;

	rayPayload.effectFlag = 0x0;
	if (Kr > 0.0f) {
		rayPayload.effectFlag = 0x1;
	}
	else if (Kt > 0.0f) {
		rayPayload.effectFlag = 0x10;
	}

	rayPayload.color = pbrColor;
	rayPayload.distance = gl_RayTmaxEXT;
	rayPayload.normal = N;
}
