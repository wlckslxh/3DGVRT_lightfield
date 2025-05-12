/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 * 
 * Full Ray Tracing
 *
 * Closest hit shader
 */

#version 460
//
//#extension GL_EXT_ray_query : enable // ihm
//#extension GL_EXT_ray_tracing : require
//#extension GL_GOOGLE_include_directive : require
//#extension GL_EXT_nonuniform_qualifier : require
//#extension GL_EXT_buffer_reference2 : require
//#extension GL_EXT_scalar_block_layout : require
//#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
//
//#include "../base/define.glsl"
//#include "../base/geometrytypes.glsl"
//#include "../base/bufferreferences.glsl"
//#include "../base/light.glsl"
//#include "../base/pbr.glsl"
//
//struct RayPayload {
//	vec3 color;	
//	float dist;	
//	vec3 normal;	
//	int effectFlag;
//};
//
//layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
//layout(location = 1) rayPayloadEXT bool shadowed;
//hitAttributeEXT vec2 attribs;
//
//layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
//
//// Initialized with default value. Appropriate value will be transfered from application.
//layout(constant_id = 0) const uint numOfLights = 1;	
//layout(constant_id = 1) const uint numOfDynamicLights = 1;
//layout(constant_id = 2) const uint numOfStaticLights = 1;
//layout(constant_id = 3) const uint staticLightOffset = 1;
//
//layout(binding = 2, set = 0) uniform uniformBuffer
//{
//	mat4 viewInverse;
//	mat4 projInverse;
//	Light lights[numOfDynamicLights];
//} ubo;
//layout(binding = 5, set = 0) uniform uniformBufferStaticLight
//{
//	Light lights[numOfStaticLights];
//} uboStaticLight;
//
//layout(binding = 4, set = 0) uniform sampler2D textures[];
//layout(binding = 6, set = 0) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;
//
//#include "../base/geometryfunctions.glsl"
//
void main()
{
//	uint nodeIndex;
//	nodeIndex = gl_GeometryIndexEXT;
//	GeometryNode geometryNode = geometryNodes.nodes[nonuniformEXT(nodeIndex)];
//
//	Triangle tri = unpackTriangle(gl_PrimitiveID, 112, geometryNode.vertexBufferDeviceAddress, geometryNode.indexBufferDeviceAddress);
//
//	if (geometryNode.textureIndexNormal > -1) {
//		tri.normal = CalculateNormal(textures[nonuniformEXT(geometryNode.textureIndexNormal)], tri.normal, tri.uv, tri.tangent);
//	}
//	
//
//	if (nonuniformEXT(geometryNode.textureIndexBaseColor) > -1) {
//		tri.color.rgb = pow(texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], tri.uv).rgb, vec3(2.2f));
//	}
//
//	vec3 aoRoughnessMetallic = vec3(1.0f);
//	if (geometryNode.textureIndexMetallicRoughness > -1) {
//		aoRoughnessMetallic = texture(textures[nonuniformEXT(geometryNode.textureIndexMetallicRoughness)], tri.uv).rgb;
//	}
//	aoRoughnessMetallic *= vec3(1.0f, geometryNode.roughnessFactor, geometryNode.metallicFactor);
//
//	rayPayload.color = vec3(0.0f);
//	if (geometryNode.textureIndexEmissive > -1){
//		rayPayload.color += texture(textures[nonuniformEXT(geometryNode.textureIndexEmissive)], tri.uv).rgb;
//	}
//
//	rayPayload.effectFlag = 0;
//	if ((geometryNode.reflectance > 0.0f && pushConstants.rayOption.reflection)) { // Kr 
//		rayPayload.effectFlag = 1 + int(nodeIndex);
//		tri.color.rgb = vec3(0.0f);
//	}
//	else if ((geometryNode.refractance > 0.0f) && pushConstants.rayOption.refraction) { // Kt
//		rayPayload.effectFlag = -1 - int(nodeIndex);
//		tri.color.rgb = vec3(0.0f);
//	}
//		
//
//	vec3 F0 = vec3(((geometryNode.ior - 1.0f) / (geometryNode.ior + 1.0f)) * ((geometryNode.ior - 1.0f) / (geometryNode.ior + 1.0f)));
//	F0 = mix(F0, tri.color.rgb, aoRoughnessMetallic.b);
//	
//	vec3 V = normalize(vec3(ubo.viewInverse * vec4(0.0f, 0.0f, 0.0f, 1.0f)) - tri.pos);
//	rayPayload.color += vec3(0.05f) * tri.color.rgb; // ambient
//
//	for(int i = 0; i < numOfLights; i++) {
//		Light light;
//		if (i < staticLightOffset)
//			light = ubo.lights[i];
//		else 
//			light = uboStaticLight.lights[i - staticLightOffset];
//
//		vec3 shadowRayDirection = light.position.xyz - tri.pos.xyz;
//		float tmax = length(shadowRayDirection);
//		if (tmax > light.radius)
//			continue;
//		shadowRayDirection /= tmax;
//
//		if (tmax >= 0.5f)
//			tmax -= 0.5f;
//		
//		vec3 shadowRayOrigin = tri.pos + SHADOW_RAY_ORIGIN_MOVEMENT_EPSILON * shadowRayDirection;
//		shadowed = false;
//		if (pushConstants.rayOption.shadowRay) {
//			shadowed = true;
//#if ANY_HIT
//			traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax, 1);
//#else
//			traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT, 0xFF, 1, 0, 1, shadowRayOrigin, RAY_TMIN, shadowRayDirection, tmax, 1);
//#endif
//		}
//		if (!shadowed) {
//			vec3 V = normalize(vec3(ubo.viewInverse[3].xyz - tri.pos));
//			vec3 L = light.position.xyz - tri.pos;
//			vec3 radiance = ApplyAttenuation(light.color, length(L), light.radius);
//			L = normalize(L);
//
//			float dotNL = max(dot(tri.normal, shadowRayDirection), 0.0f);
//			float dotNV = max(dot(tri.normal, V), 0.0f);
//			vec3 H = normalize(V + shadowRayDirection);
//
//			vec3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);
//			vec3 specular = ( DistributionGGX(tri.normal, H, aoRoughnessMetallic.g)
//									* GeometrySmith(dotNV, dotNL, aoRoughnessMetallic.g) * F)
//									/ (4.0f * dotNV * dotNL + 0.0001f);
//
//			vec3 kD = vec3(1.0f) - F;	// kS = F
//	
//			kD = kD - kD * aoRoughnessMetallic.b;	       
//
//			rayPayload.color += (kD * tri.color.rgb / PI + specular) * radiance * dotNL; // direct lighting
//		}
//    }
// 
//	rayPayload.dist = gl_RayTmaxEXT;
//	rayPayload.normal = tri.normal;
}