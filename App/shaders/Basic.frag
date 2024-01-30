#version 460

layout(location = 0) in vec2 in_uvs;
layout(location = 1) in vec4 in_fragment_position;
layout(location = 2) in vec4 in_shadow_pos;
layout(location = 3) in vec4 in_colour;
layout(location = 4) in vec3 in_normals;
layout(location = 5) in vec3 in_tangent;
layout(location = 6) in vec3 in_bitangents;

layout(location = 0) out vec4 out_colour;

layout(std140, set = 0, binding = 1) uniform ShadowData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
}
shadow;

layout(std140, set = 0, binding = 0) uniform RendererData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
  vec4 light_pos;
  vec4 light_dir;
  vec4 camera_pos;
}
renderer;

layout(push_constant) uniform PushConstants {
  vec4 albedo_colour;
  float emission;
  float metalness;
  float roughness;
  float use_normal_map;
}
pc;

layout(set = 1, binding = 9) uniform sampler2D shadow_map;
layout(set = 1, binding = 10) uniform sampler2D albedo_map;
layout(set = 1, binding = 11) uniform sampler2D diffuse_map;
layout(set = 1, binding = 12) uniform sampler2D normal_map;
layout(set = 1, binding = 13) uniform sampler2D metallic_map;
layout(set = 1, binding = 14) uniform sampler2D roughness_map;
layout(set = 1, binding = 15) uniform sampler2D ao_map;
layout(set = 1, binding = 16) uniform sampler2D specular_map;

vec3 gamma_correct(vec3 colour) { return pow(colour, vec3(1.0 / 2.2)); }

void main() {
  vec3 albedo = texture(albedo_map, in_uvs).rgb;
  vec3 diffuse_value = texture(diffuse_map, in_uvs).rgb;
  // vec3 normal = texture(normal_map, in_uvs).rgb;
  vec3 normal = normalize(in_normals);
  if (pc.use_normal_map > 0.0F) {
    // sample normal map
    vec3 tangent_normal = texture(normal_map, in_uvs).rgb;
    tangent_normal = normalize(tangent_normal * 2.0 - 1.0);
  }

  float metallic = texture(metallic_map, in_uvs).r;
  float roughness = texture(roughness_map, in_uvs).r * pc.roughness;

  float ao = texture(ao_map, in_uvs).r;

  // Ambient
  vec3 ambient = vec3(0.15) * albedo * ao;

  // Diffuse
  vec3 light_dir = normalize(renderer.light_pos.xyz - in_fragment_position.xyz);
  float NdotL = max(dot(normal, light_dir), 0.0);
  vec3 diffuse = diffuse_value * NdotL;

  // Specular
  vec3 view_dir = normalize(renderer.camera_pos.xyz - in_fragment_position.xyz);
  vec3 halfway_dir = normalize(light_dir + view_dir);
  float NdotH = max(dot(normal, halfway_dir), 0.0);
  float roughness2 = roughness * roughness;
  float NdotH2 = NdotH * NdotH;
  float denom = (NdotH2 * (roughness2 - 1.0) + 1.0);
  float D = roughness2 / (3.14159265359 * denom * denom);
  float VdotH = max(dot(view_dir, halfway_dir), 0.0);
  float VdotH2 = VdotH * VdotH;
  float F = 0.5 + 2.0 * VdotH2 * roughness2;
  float specular = F * D;

  // Combine this specular this value with the specular map
  vec3 specular_value = texture(specular_map, in_uvs).rgb;
  vec3 specular_colour = specular * specular_value;

  // Shadow

  vec4 shadow_pos = shadow.view_projection * vec4(in_shadow_pos.xyz, 1.0);
  shadow_pos.xyz /= shadow_pos.w;
  shadow_pos.xyz = shadow_pos.xyz * 0.5 + 0.5;
  float shadow_depth = texture(shadow_map, shadow_pos.xy).r;
  float visibility = shadow_pos.z > shadow_depth + 0.005 ? 0.1 : 1.0;

  // Final colour
  vec3 colour = ambient + visibility * (diffuse + specular_colour);

  out_colour = vec4(gamma_correct(colour), 1.0);
}
