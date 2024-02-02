#version 460

// Taken verbatim (minus my uniforms)
// from https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/

#include <ShaderResources.glsl>

layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;
layout(location = 0) out vec4 outColor;

vec4 calculate_grid(vec3 fragPos3D, float scale)
{
  vec2 coord = fragPos3D.xz * scale;
  vec2 derivative = fwidth(coord);
  vec2 g = abs(fract(coord - 0.5) - 0.5) / derivative;
  float line = min(g.x, g.y);
  float minimumz = min(derivative.y, 1);
  float minimumx = min(derivative.x, 1);
  vec4 colour = grid.grid_colour;
  colour.a = 1.0 - min(line, 1.0);
  // z axis
  if (fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx)
    colour.z = 1.0;
  // x axis
  if (fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz)
    colour.x = 1.0;
  return colour;
}
float computeDepth(vec3 pos)
{
  vec4 clip_space_pos = renderer.view_projection * vec4(pos.xyz, 1.0);
  return (clip_space_pos.z / clip_space_pos.w);
}
float computeLinearDepth(vec3 pos)
{
  vec4 clip_space_pos = renderer.view_projection * vec4(pos.xyz, 1.0);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0; // put back between -1 and 1
    float near = grid.grid_size.z; // near plane
    float far = grid.grid_size.a; // far plane
    float linearDepth = (2.0 * near * far) / (far + near - clip_space_depth * (far - near)); // get linear value between 0.01 and 100
    return linearDepth / far;
}
void main()
{
  float t = -nearPoint.y / (farPoint.y - nearPoint.y);
  vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);

  gl_FragDepth = computeDepth(fragPos3D);

  float linearDepth = computeLinearDepth(fragPos3D);
  float fading = max(0, (0.5 - linearDepth));

  outColor = (calculate_grid(fragPos3D, grid.grid_size.x) +
              calculate_grid(fragPos3D, grid.grid_size.y)) * float(t > 0);
  outColor.a *= fading;
}
