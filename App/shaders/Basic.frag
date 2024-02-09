#version 460

#include <PBRUtility.glsl>
#include <ShaderResources.glsl>
#include <ShadowCalculation.glsl>

layout(set = 1, binding = 29) uniform samplerCube u_EnvIrradianceTex;
layout(set = 1, binding = 30) uniform samplerCube u_EnvRadianceTex;

layout(location = 0) in vec2 in_uvs;
layout(location = 1) in vec4 in_fragment_pos;
layout(location = 2) in vec4 in_shadow_pos;
layout(location = 3) in vec3 in_normals;
layout(location = 4) in vec4 in_colour;
layout(location = 5) in mat3 in_tbn;

layout(location = 0) out vec4 out_colour;

vec3 gamma_correct(vec3 color) { return pow(color, vec3(1.0 / 2.2)); }

vec3 getAlbedo() {
  vec4 albedo_colour = pc.albedo_colour;
  vec3 sampled = texture(albedo_map, in_uvs).rgb;
  return albedo_colour.rgb * sampled * in_colour.rgb;
}

float getMetalness() {
  float metalness_from_texture = texture(metallic_map, in_uvs).r;
  return metalness_from_texture * pc.metalness;
}

float getRoughness() {
  float roughness_from_texture = texture(roughness_map, in_uvs).r;
  float computed = roughness_from_texture * pc.roughness;
  return max(computed, 0.05);
}

float getAO() { return texture(ao_map, in_uvs).r; }

vec3 getNormal() {
  if (pc.use_normal_map < 0)
    return in_normals;

  vec3 normalFromMap = texture(normal_map, in_uvs).rgb;
  normalFromMap = normalFromMap * 2.0 - 1.0; // Transform from [0,1] to [-1,1]

  // Use the TBN matrix to transform the normal from tangent to world space
  vec3 worldNormal = normalize(in_tbn * normalFromMap);

  return worldNormal;
}

void main() {
  vec3 albedo = getAlbedo();
  float metalness = getMetalness();
  float roughness = getRoughness();
  float ao = getAO();
  vec3 normal = getNormal();
  vec3 viewDir = normalize(renderer.camera_pos.xyz - in_fragment_pos.xyz);

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

  // Calculate soft shadow factor
  vec3 projCoords = in_shadow_pos.xyz / in_shadow_pos.w;
  vec2 shadowMapSize = vec2(4096, 4096); // Example, use actual size
  float shadowFactor = calculateSoftShadow(
      shadow_map, projCoords, normal, normalize(-renderer.light_dir.xyz),
      shadowMapSize, 3); // Example filter size of 3

  // Incorporate the shadow factor into the diffuse and specular components
  vec3 litColor = (diffuse + specular) * lightColor * shadowFactor;

  // Combine results with ambient lighting unaffected by shadows
  vec3 ambient = vec3(0.03) * albedo * ao;
  vec3 iblContribution =
      IBL(u_EnvIrradianceTex, u_EnvRadianceTex, normal, viewDir, F0, albedo,
          roughness, metalness, 20.0F, reflect(-viewDir, normal));
  vec3 color = ambient + litColor + iblContribution;

  out_colour = vec4(gamma_correct(color), texture(albedo_map, in_uvs).a);
}
