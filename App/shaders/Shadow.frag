#version 460

layout(std140, set = 0, binding = 1) uniform ShadowData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
  vec2 bias_and_default;
}
shadow;

void main() {}
