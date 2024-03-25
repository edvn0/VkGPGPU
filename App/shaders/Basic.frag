#version 460

#include <PBRUtility.glsl>
#include <ShaderResources.glsl>
#include <ShadowCalculation.glsl>

layout(push_constant) uniform PushConstants {
  vec4 albedo_colour;
  float emission;
  float metalness;
  float roughness;
  float use_normal_map;
}
pc;

layout(set = 1, binding = 29) uniform samplerCube irradiance_texture;
layout(set = 1, binding = 30) uniform samplerCube radiance_texture;
layout(set = 1, binding = 31) uniform sampler2D brdf_lookup;

layout(location = 0) in vec2 in_uvs;
layout(location = 1) in vec4 in_fragment_pos;
layout(location = 2) in vec4 in_shadow_pos;
layout(location = 3) in vec4 in_colour;
layout(location = 4) in vec3 in_normals;
layout(location = 5) in mat3 in_normal_matrix;

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

vec3 getSpecularIBL(vec3 N, vec3 V, vec3 F0, float roughness) {
  const float MAX_REFLECTION_LOD = 4.0;
  vec3 R = reflect(-V, N); // Reflection vector
  vec3 prefilteredColor =
      textureLod(radiance_texture, R, roughness * MAX_REFLECTION_LOD).rgb;
  vec2 brdf = texture(brdf_lookup, vec2(max(dot(N, V), 0.0), roughness)).rg;
  vec3 specular = prefilteredColor * (F0 * brdf.x + brdf.y);
  return specular;
}

vec3 getAmbientIBL(vec3 N, vec3 albedo) {
  vec3 irradiance = texture(irradiance_texture, N).rgb;
  // The constant term PI is cancelled out in the division below
  vec3 ambient = irradiance * albedo;
  return ambient;
}

vec3 getNormal() {
  if (pc.use_normal_map <= 0.0F)
    return normalize(in_normals);

  vec3 world_normal_from_map = texture(normal_map, in_uvs).rgb * 2.0f - 1.0f;
  world_normal_from_map = normalize(world_normal_from_map * in_normals);

  return world_normal_from_map;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;

  float num = a2;
  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return num / denom;
}

float geometrySchlickGGX(float NdotV, float roughness) {
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float num = NdotV;
  float denom = NdotV * (1.0 - k) + k;

  return num / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx1 = geometrySchlickGGX(NdotV, roughness);
  float ggx2 = geometrySchlickGGX(NdotL, roughness);
  return ggx1 * ggx2;
}

void main() {
  vec3 albedo = getAlbedo();
  float metalness = getMetalness();
  float roughness = getRoughness();
  vec3 normal = getNormal();
  vec3 viewDir = normalize(renderer.camera_pos.xyz - in_fragment_pos.xyz);

  vec3 F0 = mix(vec3(0.04), albedo, metalness); // Base reflectivity
  vec3 L = normalize(renderer.light_pos.xyz - in_fragment_pos.xyz);
  vec3 H = normalize(viewDir + L);
  float NdotL = max(dot(normal, L), 0.0);
  vec3 radiance = renderer.light_colour.rgb * renderer.light_colour.a;

  // Fresnel
  float cosTheta = max(dot(normal, viewDir), 0.0);
  vec3 F = fresnelSchlick(cosTheta, F0);

  // Normal Distribution
  float D = distributionGGX(normal, H, roughness);

  // Geometry
  float G = geometrySmith(normal, viewDir, L, roughness);

  // Specular BRDF
  vec3 numerator = D * F * G;
  float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * NdotL +
                      0.001; // Prevent division by zero
  vec3 specular = numerator / denominator;

  // Combine diffuse and specular
  vec3 kD = vec3(1.0) - F; // Diffuse component (non-metallic)
  kD *= 1.0 - metalness;   // Metals do not have a diffuse component

  vec3 diffuse = (albedo / PI) * NdotL;         // Lambertian diffuse
  vec3 ambient = vec3(0.03) * albedo * getAO(); // Ambient light

  vec3 direct = kD * diffuse + specular;

  // Adjusting the x and y coordinates remains the same, mapping from [-1, 1] to
  // [0, 1]
  vec4 xy_coords_remapped = (in_shadow_pos / in_shadow_pos.w);
// The z coordinate is the depth value, which is in [0, 1] range
  float shadow = 1.0F;
  if (texture(shadow_map, xy_coords_remapped.xy).r < xy_coords_remapped.z - 0.005)
	shadow = 0.0F;


  vec3 final = ambient + (1.0F - shadow) * direct * radiance;

  out_colour = vec4(gamma_correct(final), 1.0F);
}
