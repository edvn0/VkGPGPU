#version 450

#include <ShaderResources.glsl>

vec3 cubeVertices[8] =
    vec3[8](vec3(-1.0, -1.0, -1.0), vec3(-1.0, -1.0, 1.0),
            vec3(-1.0, 1.0, -1.0), vec3(-1.0, 1.0, 1.0), vec3(1.0, -1.0, -1.0),
            vec3(1.0, -1.0, 1.0), vec3(1.0, 1.0, -1.0), vec3(1.0, 1.0, 1.0));

struct CubeInstance {
  mat4 transform;
  vec4 colour;
  vec4 side_length;
};
layout(std140, set = 0, binding = 21) readonly buffer CubeInstances {
  CubeInstance cubes[];
}
cubes;

layout(location = 0) out vec4 out_colour;

void main() {
  CubeInstance cube = cubes.cubes[gl_InstanceIndex];

  int tri = gl_VertexIndex / 3;
  int idx = gl_VertexIndex % 3;
  int face = tri / 2;
  int top = tri % 2;

  int dir = face % 3;
  int pos = face / 3;

  int nz = dir >> 1;
  int ny = dir & 1;
  int nx = 1 ^ (ny | nz);

  vec3 d = vec3(nx, ny, nz);
  float flip = 1 - 2 * pos;

  vec3 n = flip * d;
  vec3 u = -d.yzx;
  vec3 v = flip * d.zxy;

  float mirror = -1 + 2 * top;
  vec3 xyz =
      n + mirror * (1 - 2 * (idx & 1)) * u + mirror * (1 - 2 * (idx >> 1)) * v;

  xyz *= cube.side_length.x;

  gl_Position = renderer.view_projection * cube.transform * vec4(xyz, 1.0);
  out_colour = cube.colour;
}
