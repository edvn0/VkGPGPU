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
  vec4 light_pos;
  vec4 light_dir;
  vec4 camera_pos;
}
shadow;

layout(set = 1, binding = 9) uniform sampler2D shadow_map;
layout(set = 1, binding = 10) uniform sampler2D albedo;
layout(set = 1, binding = 11) uniform sampler2D diffuse_map;
layout(set = 1, binding = 12) uniform sampler2D normal;
layout(set = 1, binding = 13) uniform sampler2D metallic;
layout(set = 1, binding = 14) uniform sampler2D roughness;
layout(set = 1, binding = 15) uniform sampler2D ao;
layout(set = 1, binding = 16) uniform sampler2D specular_map;

vec3 gamma_correct(vec3 colour) {
  return pow(colour, vec3(1.0 / 2.2));
}

void main() {

 vec3 albedo = texture(albedo, in_uvs).rgb;
  vec3 diffuse_value = texture(diffuse_map, in_uvs).rgb;
  vec3 normal = texture(normal, in_uvs).rgb;
  // normal may just be 1,1,1 from the texture (no normal map)
  if (normal == vec3(1.0)) {
    normal = in_normals;
  }
  float metallic = texture(metallic, in_uvs).r;
  float roughness = texture(roughness, in_uvs).r;
  float ao = texture(ao, in_uvs).r;

  // Ambient
  vec3 ambient = vec3(0.03) * albedo * ao;

  // Diffuse
  vec3 light_dir = normalize(shadow.light_pos.xyz - in_shadow_pos.xyz);
  float NdotL = max(dot(normal, light_dir), 0.0);
  vec3 diffuse = NdotL * albedo * diffuse_value;

  // Specular
  vec3 view_dir = normalize(shadow.camera_pos.xyz - in_shadow_pos.xyz);
  vec3 halfway_dir = normalize(-light_dir + view_dir);
  float NdotH = max(dot(normal, halfway_dir), 0.0);
  float roughness2 = roughness * roughness;
  float NdotH2 = NdotH * NdotH;
  float denom = (NdotH2 * (roughness2 - 1.0) + 1.0);
  float D = roughness2 / (3.14159265359 * denom * denom);
  float VdotH = max(dot(view_dir, halfway_dir), 0.0);
  float VdotH2 = VdotH * VdotH;
  float NdotV = max(dot(normal, view_dir), 0.0);
  float F = 0.5 + 2.0 * VdotH2 * roughness2;
  float G = min(1.0, min(2.0 * NdotH * NdotV / VdotH, 2.0 * NdotH * NdotL / VdotH));
  vec3 specular = F * G * D * albedo * metallic;

  // Shadow
  float shadow = 0.0;
  vec4 shadow_pos = in_shadow_pos / in_shadow_pos.w;
  if (shadow_pos.z > 1.0) {
    shadow = 0.0;
  }
  else {
    shadow_pos.z += 0.0001;
    shadow = texture(shadow_map, shadow_pos.xy).r;
    if (shadow < shadow_pos.z) {
      shadow = 0.0;
    }
    else {
      shadow = 1.0;
    }
  }

  // Final colour
  vec3 colour = ambient + (1.0 - shadow) * (diffuse + specular);

  out_colour = vec4(gamma_correct(colour), 1.0);
}
