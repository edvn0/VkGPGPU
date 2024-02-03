#version 460

#include <ShaderResources.glsl>

layout(location = 0) in vec2 in_uvs;
layout(location = 1) in vec4 in_fragment_position;
layout(location = 2) in vec4 in_shadow_pos;
layout(location = 3) in vec4 in_colour;
layout(location = 4) in vec3 in_normals;
layout(location = 5) in vec3 in_tangent;
layout(location = 6) in vec3 in_bitangents;
layout(location = 7) in mat3 in_tbn;

layout(location = 0) out vec4 out_colour;

vec3 gamma_correct(vec3 colour) { return pow(colour, vec3(1.0 / 2.2)); }

void main()
{
  // Ambient light, sampled from the uniform sampler2D ambient_map
  vec4 ambient_texture = texture(albedo_map, in_uvs);
  if (ambient_texture.a < 0.1)
    discard;
  // Also use the pc.albedo_colour
  vec3 ambient = ambient_texture.xyz * in_colour.rgb;

  // Specular light, sampled from the uniform sampler2D specular_map
  vec3 specular = texture(specular_map, in_uvs).rgb;

  // Normal, sampled from the uniform sampler2D normal_map
  vec3 normal = in_normals;
  if (pc.use_normal_map > 0)
  {
    normal = texture(normal_map, in_uvs).rgb;
    normal = normal * 2.0 - 1.0;
    normal = normalize(in_tbn * normal);
  }

  // Diffuse
  vec3 diffuse = vec3(0.0);
  vec3 light_dir = normalize(renderer.light_dir.xyz);
  float diff = max(dot(normal, light_dir), 0.0);
  diffuse = diff * in_colour.rgb;

  // Specular
  vec3 view_direction =
      normalize(renderer.camera_pos.xyz - in_fragment_position.xyz);
  vec3 half_direction = normalize(renderer.light_dir.xyz + view_direction);
  const float roughness = pc.roughness;
  float specular_intensity = pow(max(dot(half_direction, normal), 0.0), 64.0F);
  specular *= specular_intensity;

  // Shadow
  vec4 shadow_pos = in_shadow_pos;
  shadow_pos.xyz /= shadow_pos.w;
  shadow_pos.xyz = shadow_pos.xyz * 0.5 + 0.5;
  float visibility = 1.0;
  if (texture(shadow_map, shadow_pos.xy).r <
      shadow_pos.z - shadow.bias_and_default.x)
  {
    visibility = shadow.bias_and_default.y;
  }

  // Final colour
  vec3 colour = ambient + (1.0 - visibility) * (diffuse + specular);

  out_colour = vec4(gamma_correct(colour), 1.0);
}
