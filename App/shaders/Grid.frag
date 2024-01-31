#version 460

#include <ShaderResources.glsl>

layout(location = 0) out vec4 out_colour;

void main()
{
  vec4 view_pos = shadow.view * vec4(gl_FragCoord.xyz, 1.0);

  vec2 grid_coordinates = mod(view_pos.xz / grid.grid_size.xy, 1.0);
  float line_thickness = 0.02;
  float gridPattern = min(smoothstep(0.0, line_thickness, grid_coordinates.x),
                          smoothstep(0.0, line_thickness, grid_coordinates.y));

  vec4 color = mix(grid.plane_colour, grid.grid_colour, gridPattern);

  float distance = length(renderer.camera_pos.xyz - gl_FragCoord.xyz);
  const float fog_density = grid.fog_colour.a;
  float fogFactor = 1.0 / exp(distance * fog_density);
  fogFactor = clamp(fogFactor, 0.0, 1.0);

  out_colour = mix(grid.fog_colour, color, fogFactor);
}
