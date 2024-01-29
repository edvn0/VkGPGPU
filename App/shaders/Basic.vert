#version 460

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

layout(std140, set = 0, binding = 0) uniform RendererData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
}
renderer;

layout(std140, set = 0, binding = 2) readonly buffer VertexTransforms {
  mat4 matrices[];
}
transforms;

layout(std140, set = 0, binding = 1) uniform ShadowData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
  vec4 light_pos;
  vec4 light_dir;
  vec4 camera_pos;
}
shadow;

void main() {
  vec4 computed = transforms.matrices[gl_InstanceIndex] * vec4(pos, 1.0F);
  gl_Position = renderer.view_projection * computed;
  out_shadow_pos = shadow.view_projection * computed;

  out_uvs = uvs;
  out_colour = colour;
  out_fragment_pos = computed;
  out_normals = normals;
  out_tangent = tangent;
  out_bitangents = bitangents;
}
