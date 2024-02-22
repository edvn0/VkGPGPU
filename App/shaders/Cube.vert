#version 450

#include <ShaderResources.glsl>

struct CubeInstance
{
  mat4 transform;
  vec4 colour;
  vec4 side_length;
};
layout(std140, set = 0, binding = 21) readonly buffer CubeInstances
{
  CubeInstance cubes[];
}
cubes;

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
layout(location = 5) out mat3 out_normal_matrix;

void main()
{
  CubeInstance cube = cubes.cubes[gl_InstanceIndex];
  mat4 transform = cube.transform;

  vec4 computed = transform * vec4(pos, 1.0F);
  gl_Position = renderer.view_projection * computed;
  out_shadow_pos = shadow.view_projection * computed;

  out_uvs = uvs;
  out_fragment_pos = computed;

  out_normal_matrix = mat3(transform) * mat3(tangent, bitangents, normals);
  out_normals = mat3(transform) * normals;
  if (all(equal(colour, vec4(1.0, 1.0, 1.0, 1.0))))
  {
    out_colour = cube.colour;
  }
  else
  {
    out_colour = colour;
  }
}
