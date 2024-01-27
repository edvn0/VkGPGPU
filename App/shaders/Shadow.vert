#version 460

layout(location = 0) in vec3 pos;

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
  gl_Position = shadow.view_projection * computed;
}
