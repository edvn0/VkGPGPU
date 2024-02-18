#version 460

#include <ShaderResources.glsl>

layout(location = 0) out vec4 out_colour; // Pass color to fragment shader

struct LineVertex {
  vec4 pos;
  vec4 col;
};
layout(std140, set = 0, binding = 20) readonly buffer LineVertices {
  LineVertex vertices[];
}
verts;

void main() {
  LineVertex vert = verts.vertices[gl_InstanceIndex];
  out_colour = vert.col; // Pass color to fragment shader
  gl_Position = renderer.view_projection *
                vec4(vert.pos.xyz, 1.0); // Transform vertex position
}
