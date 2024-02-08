#ifndef GPGU_PBR_UTILITY
#define GPGU_PBR_UTILITY

// Basic PBR constants
const float PI = 3.141592653589793;

vec3 FresnelSchlickRoughness(vec3 F0, float cosTheta, float roughness) {
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 RotateVectorAboutY(float angle, vec3 v) {
  float c = cos(angle);
  float s = sin(angle);
  return vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

// Cook-Torrance BRDF components
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;

  float nom = a2;
  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return nom / denom;
}

float geometrySchlickGGX(float NdotV, float roughness) {
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float nom = NdotV;
  float denom = NdotV * (1.0 - k) + k;

  return nom / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx2 = geometrySchlickGGX(NdotV, roughness);
  float ggx1 = geometrySchlickGGX(NdotL, roughness);

  return ggx1 * ggx2;
}

vec3 IBL(samplerCube envIrradianceTex, samplerCube envRadianceTex, vec3 normal,
         vec3 viewDir, vec3 F0, vec3 albedo, float roughness, float metalness,
         float envMapRotation, vec3 Lr) {
  vec3 irradiance = texture(envIrradianceTex, normal).rgb;
  float NdotV = max(dot(normal, viewDir), 0.0);
  vec3 F = FresnelSchlickRoughness(F0, NdotV, roughness);
  vec3 kd = (1.0 - F) * (1.0 - metalness);
  vec3 diffuseIBL = albedo * irradiance;

  int envRadianceTexLevels = textureQueryLevels(envRadianceTex);
  vec3 R = 2.0 * dot(viewDir, normal) * normal - viewDir;
  vec3 specularIrradiance =
      textureLod(envRadianceTex, RotateVectorAboutY(envMapRotation, Lr),
                 roughness * float(envRadianceTexLevels))
          .rgb;

  vec3 specularIBL = specularIrradiance;

  return kd * diffuseIBL + specularIBL;
}

#endif // GPGU_PBR_UTILITY
