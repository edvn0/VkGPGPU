#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uvs;
layout(location = 2) in vec4 colour;
layout(location = 3) in vec3 normals;
layout(location = 4) in vec3 tangent;
layout(location = 5) in vec3 bitangents;

layout(location = 0) out vec2 out_uvs;
layout(location = 1) out vec4 shadow_pos;

layout(std140, set = 2, binding = 0) uniform RendererData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
}
renderer;

layout(std140, set = 2, binding = 1) readonly buffer VertexTransforms {
  mat4 matrices[];
}
transforms;

layout(std140, set = 2, binding = 2) uniform ShadowData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
}
shadow;

void main() {
  out_uvs = uvs;
  vec4 computed = transforms.matrices[gl_InstanceIndex] * vec4(pos, 1.0F);
  gl_Position = renderer.view_projection * computed;
  shadow_pos = shadow.view_projection * computed;
}
