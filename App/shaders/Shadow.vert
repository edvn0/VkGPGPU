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

void main() {
  mat4 transform = mat4(
      vec4(transform_row_zero.x, transform_row_one.x, transform_row_two.x, 0.0),
      vec4(transform_row_zero.y, transform_row_one.y, transform_row_two.y, 0.0),
      vec4(transform_row_zero.z, transform_row_one.z, transform_row_two.z, 0.0),
      vec4(transform_row_zero.w, transform_row_one.w, transform_row_two.w,
           1.0));
  vec4 computed = transform * vec4(pos, 1.0F);
  gl_Position = shadow.view_projection * computed;
}
