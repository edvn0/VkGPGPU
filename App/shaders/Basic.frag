#version 460

#include <ShaderResources.glsl>

// Assuming previous definitions and buffer bindings are included here

layout(location = 0) in vec2 in_uvs;
layout(location = 1) in vec4 in_fragment_position;
layout(location = 2) in vec4 in_shadow_pos;
layout(location = 3) in vec3 in_normals;
layout(location = 4) in vec3 in_tangent;
layout(location = 5) in vec3 in_bitangent;
layout(location = 6) in mat3 in_tbn; // Tangent, Bitangent, Normal matrix

layout(location = 0) out vec4 out_colour;

vec3 gamma_correct(vec3 color) { return pow(color, vec3(1.0 / 2.2)); }

// Basic PBR constants
const float PI = 3.141592653589793;

vec3 getAlbedo() { return texture(albedo_map, in_uvs).rgb; }

float getMetalness() {
  float metalness_from_texture = texture(metallic_map, in_uvs).r;
  return metalness_from_texture * pc.metalness;
}

float getRoughness() {
  float roughness_from_texture = texture(roughness_map, in_uvs).r;
  return roughness_from_texture * pc.roughness;
}

float getAO() { return texture(ao_map, in_uvs).r; }

vec3 getNormal() {
  vec3 normal = normalize(in_normals);
  if (pc.use_normal_map > 0) {
    vec3 tNormal = texture(normal_map, in_uvs).rgb;
    tNormal = normalize(tNormal);         // Convert to NDC
    normal = normalize(in_tbn * tNormal); // Transform to world space
  }
  return normal;
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

void main() {
  vec3 albedo = getAlbedo();
  float metalness = getMetalness();
  float roughness = getRoughness();
  float ao = getAO();
  vec3 normal = getNormal();
  vec3 viewDir = normalize(renderer.camera_pos.xyz - in_fragment_position.xyz);

  // Light model

  vec3 lightColor = renderer.light_colour.rgb;
  vec3 lightDir = -normalize(renderer.light_dir.xyz);
  float NDF, G;
  vec3 kS, kD, specular;

  vec3 F0 = vec3(0.04);
  F0 = mix(F0, albedo, metalness);

  vec3 halfDir = normalize(viewDir + lightDir);
  float cosTheta = max(dot(normal, lightDir), 0.0);

  NDF = distributionGGX(normal, halfDir, roughness);
  G = geometrySmith(normal, viewDir, lightDir, roughness);
  vec3 F = fresnelSchlick(max(dot(halfDir, viewDir), 0.0), F0);

  vec3 nominator = NDF * G * F;
  float denominator =
      4 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) +
      0.001;
  specular = nominator / denominator;

  kS = F;
  kD = vec3(1.0) - kS;
  kD *= 1.0 - metalness;

  vec3 numerator = cosTheta * albedo * PI;
  vec3 diffuse = numerator * kD;

  // Combine results
  vec3 ambient = vec3(0.03) * albedo * ao;
  vec3 color = ambient + (diffuse + specular) * lightColor;

  out_colour = vec4(gamma_correct(color), 1.0);
}
