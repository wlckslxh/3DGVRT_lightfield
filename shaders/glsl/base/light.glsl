/*
 * Sogang University
 *
 * Graphics Lab, 2024
 *
 */

 //[prev]
// LIGHT TYPE
//#define POINT_LIGHT 0
//#define DIRECTIONAL_LIGHT 1
//#define SPOT_LIGHT 2

struct SpotLight {
	float innerConeAngle;
	float outerConeAngle;
};

struct Light {
	vec3 position;
	float radius;
	vec3 color;
	uint type;
};

vec3 spotDir = vec3(0.0f, -1.0f, 0.0f);

//float CalculateSpotLight(vec3 pos, Light light)
//{
//    // new updating version
//    float lightAngleScale = 1.0f / max(0.001f, cos(light.spot.innerConeAngle) - cos(light.spot.outerConeAngle));
//    float lightAngleOffset = -cos(light.spot.outerConeAngle) * lightAngleScale;
//
//    float spotFactor = dot(spotDir, normalize(pos - light.position.xyz));
//    float angularAttenuation = clamp(spotFactor * lightAngleScale + lightAngleOffset, 0.0f, 1.0f); 
//    angularAttenuation *= angularAttenuation;
//    return angularAttenuation;
//}