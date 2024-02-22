#ifndef BUFFER_VKGPU
#define BUFFER_VKGPU

mat4 depth_bias = mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5,
                       0.0, 0.5, 0.5, 0.5, 1.0);

layout(std140, set = 0, binding = 1) uniform ShadowData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
  vec2 bias_and_default;
}
shadow;

layout(std140, set = 0, binding = 0) uniform RendererData {
  mat4 view;
  mat4 projection;
  mat4 view_projection;
  mat4 inverse_view;
  mat4 inverse_projection;
  mat4 inverse_view_projection;
  vec4 light_pos;
  vec4 light_dir;
  vec4 camera_pos;
  vec4 light_colour;
  vec4 specular_colour;
}
renderer;

layout(std140, set = 0, binding = 2) readonly buffer VertexTransforms {
  mat4 matrices[];
}
transforms;

layout(std140, set = 0, binding = 3) uniform GridData {
  vec4 grid_colour;
  vec4 plane_colour;
  vec4 grid_size;
  vec4 fog_colour;
}
grid;

layout(set = 1, binding = 9) uniform sampler2D shadow_map;
layout(set = 1, binding = 10) uniform sampler2D albedo_map;
layout(set = 1, binding = 11) uniform sampler2D diffuse_map;
layout(set = 1, binding = 12) uniform sampler2D normal_map;
layout(set = 1, binding = 13) uniform sampler2D metallic_map;
layout(set = 1, binding = 14) uniform sampler2D roughness_map;
layout(set = 1, binding = 15) uniform sampler2D ao_map;

#endif
