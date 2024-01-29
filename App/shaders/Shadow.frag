#version 460

layout(std140, set = 0, binding = 1) uniform ShadowData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
  vec4 light_pos;
  vec4 light_dir;
  vec4 camera_pos;
}
shadow;

void main() {}
