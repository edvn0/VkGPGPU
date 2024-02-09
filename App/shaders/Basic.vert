#version 460

#include <ShaderResources.glsl>

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uvs;
layout(location = 2) in vec4 colour;
layout(location = 3) in vec3 normals;
layout(location = 4) in vec3 tangent;
layout(location = 5) in vec3 bitangents;

layout(location = 6) in vec4 transform_row_zero;
layout(location = 7) in vec4 transform_row_one;
layout(location = 8) in vec4 transform_row_two;

layout(location = 0) out vec2 out_uvs;
layout(location = 1) out vec4 out_fragment_pos;
layout(location = 2) out vec4 out_shadow_pos;
layout(location = 3) out vec3 out_normals;
layout(location = 4) out vec4 out_colour;
layout(location = 5) out mat3 out_tbn;

void main() {
  mat4 transform = mat4(
      vec4(transform_row_zero.x, transform_row_one.x, transform_row_two.x, 0.0),
      vec4(transform_row_zero.y, transform_row_one.y, transform_row_two.y, 0.0),
      vec4(transform_row_zero.z, transform_row_one.z, transform_row_two.z, 0.0),
      vec4(transform_row_zero.w, transform_row_one.w, transform_row_two.w,
           1.0));

  vec4 computed = transform * vec4(pos, 1.0F);
  gl_Position = renderer.view_projection * computed;
  out_shadow_pos = shadow.view_projection * computed;

  out_uvs = uvs;
  out_fragment_pos = computed;
  // Calculate TBN
  vec3 T = normalize(computed * vec4(tangent, 0.0F)).xyz;
  vec3 N = normalize(computed * vec4(normals, 0.0F)).xyz;
  vec3 B = normalize(computed * vec4(bitangents, 0.0F)).xyz;
  out_tbn = mat3(T, B, N);

  out_normals = normals;
  out_colour = colour;
}
