#version 460

#include <ShaderResources.glsl>

layout(location = 0) out vec4 colour;

struct LineInstance
{
  vec4 start_pos;
  vec4 end_pos;
  vec4 col;
};
layout(std140, set = 0, binding = 20) readonly buffer LineVertices
{
  LineInstance vertices[];
}
verts;

void main()
{
  LineInstance line_instance = verts.vertices[gl_InstanceIndex];

  vec4 position = (gl_VertexIndex % 2 == 0) ? line_instance.start_pos
                                            : line_instance.end_pos;
  vec4 computed = vec4(position.xyz, 1.0F);

  gl_Position = renderer.view_projection * computed;
  colour = line_instance.col;
}
