#version 460

layout(location = 0) in vec2 in_uvs;
layout(location = 1) in vec4 shadow_pos;

layout(location = 0) out vec4 colour;

void main() {
  colour = vec4(in_uvs, 1.0F, 1.0F);
}
