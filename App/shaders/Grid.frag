#version 460

layout (location = 0) out vec4 out_colour;

layout (std140, set = 0, binding = 3) uniform GridData {
    vec4 grid_colour;
    vec4 plane_colour;
    vec4 grid_size;
    vec4 fog_colour;
} grid;


layout(std140, set = 0, binding = 1) uniform ShadowData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
  vec4 light_pos;
  vec4 light_dir;
  vec4 camera_pos;
}
shadow;

void main() {
 // Transform the fragment position to view space
    vec4 viewPos = shadow.view * vec4(gl_FragCoord.xyz, 1.0);

    // Calculate the grid pattern in view space
    vec2 gridCoord = mod(viewPos.xz / grid.grid_size.xy, 1.0);
    float lineThickness = 0.02; // You can adjust this value as needed
    float gridPattern = min(smoothstep(0.0, lineThickness, gridCoord.x),
                            smoothstep(0.0, lineThickness, gridCoord.y));

    // Choose color based on grid pattern
    vec4 color = mix(grid.plane_colour, grid.grid_colour, gridPattern);

    // Fog logic (as before)
    float distance = length(shadow.camera_pos.xyz - gl_FragCoord.xyz);
    const float fog_density = grid.fog_colour.a;
    float fogFactor = 1.0 / exp(distance * fog_density);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    // Mix final color with fog
    out_colour = mix(grid.fog_colour, color, fogFactor);
}
