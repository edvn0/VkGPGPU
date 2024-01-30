#version 460

layout(location = 0) out vec4 out_colour;

layout(std140, set = 0, binding = 3) uniform GridData {
  vec4 grid_colour;
  vec4 plane_colour;
  vec4 grid_size;
  vec4 fog_colour;
}
grid;

layout(std140, set = 0, binding = 1) uniform ShadowData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
}
shadow;

layout(std140, set = 0, binding = 0) uniform RendererData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
  vec4 light_pos;
  vec4 light_dir;
  vec4 camera_pos;
}
renderer;

void main() {
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
