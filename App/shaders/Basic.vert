#version 460

#include <ShaderResources.glsl>

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uvs;
layout(location = 2) in vec4 colour;
layout(location = 3) in vec3 normals;
layout(location = 4) in vec3 tangent;
layout(location = 5) in vec3 bitangents;

layout(location = 0) out vec2 out_uvs;
layout(location = 1) out vec4 out_fragment_pos;
layout(location = 2) out vec4 out_shadow_pos;
layout(location = 3) out vec4 out_colour;
layout(location = 4) out vec3 out_normals;
layout(location = 5) out vec3 out_tangent;
layout(location = 6) out vec3 out_bitangents;
layout(location = 7) out mat3 out_tbn;

void main()
{
  vec4 computed = transforms.matrices[gl_InstanceIndex] * vec4(pos, 1.0F);
  gl_Position = renderer.view_projection * computed;
  out_shadow_pos = shadow.view_projection * computed;

  out_uvs = uvs;
  out_colour = colours.matrices[gl_InstanceIndex] * colour;
  out_fragment_pos = computed;
  // Calculate TBN
  vec3 T = normalize(computed * vec4(tangent, 0.0F)).xyz;
  vec3 N = normalize(computed * vec4(normals, 0.0F)).xyz;
  vec3 B = normalize(computed * vec4(bitangents, 0.0F)).xyz;
  mat3 TBN = transpose(mat3(T, B, N));
  out_tbn = TBN;

  out_normals = normals;
  out_tangent = tangent;
  out_bitangents = bitangents;
}
