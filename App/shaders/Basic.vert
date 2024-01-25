#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uvs;

layout(location = 0) out vec2 out_uvs;

void main() {
  out_uvs = uvs;
  gl_Position = vec4(pos, 1.0F);
}
