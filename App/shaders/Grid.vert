#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uvs;
layout(location = 2) in vec4 colour;
layout(location = 3) in vec3 normals;
layout(location = 4) in vec3 tangent;
layout(location = 5) in vec3 bitangents;

layout(std140, set = 0, binding = 0) uniform RendererData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
}
renderer;

void main()
{
    gl_Position = renderer.view_projection * vec4(pos, 1.0);
}
