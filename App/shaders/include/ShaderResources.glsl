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
  vec4 camera_pos;
  vec4 light_pos;
  vec4 light_dir;
  vec4 light_colour;
  vec4 specular_colour;
  vec2 ndc_to_view_multiplied;
  vec2 ndc_to_view_added;
  vec2 depth_unpacked_constants;
  vec2 camera_tan_half_fov;
  ivec2 tiles_count;
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

struct PointLight {
  vec3 pos;
  float multiplier;
  vec3 radiance;
  float min_radius;
  float radius;
  float falloff;
  float light_size;
  bool casts_shadows;
};

#define TILE_SIZE 16
#define MAX_LIGHT_COUNT 1000

layout(std140, set = 0, binding = 4) uniform PointLightData {
  uint count;
  PointLight lights[MAX_LIGHT_COUNT];
}
point_lights;

struct SpotLight {
  vec3 pos;
  float multiplier;
  vec3 direction;
  float angle_attenuation;
  vec3 radiance;
  float range;
  float angle;
  float falloff;
  bool soft_shadows;
  bool casts_shadows;
};

layout(std140, set = 0, binding = 5) uniform SpotLightData {
  uint count;
  SpotLight lights[MAX_LIGHT_COUNT];
}
spot_lights;

layout(std140, set = 0, binding = 6) uniform SpotShadowData {
  mat4 matrices[MAX_LIGHT_COUNT];
}
spot_light_matrices;

layout(std430, set = 0, binding = 7) buffer VisiblePointLightIndicesBuffer {
  int indices[];
}
visible_point_light_buffer;

layout(std430, set = 0,
       binding = 8) writeonly buffer VisibleSpotLightIndicesBuffer {
  int indices[];
}
visible_spot_light_buffer;

layout(std140, set = 0, binding = 9) uniform ScreenData {
  vec2 inverse_full_resolution;
  vec2 full_resolution;
  vec2 inverse_half_resolution;
  vec2 half_resolution;
}
screen_data;

layout(set = 1, binding = 9) uniform sampler2D shadow_map;
layout(set = 1, binding = 10) uniform sampler2D albedo_map;
layout(set = 1, binding = 11) uniform sampler2D diffuse_map;
layout(set = 1, binding = 12) uniform sampler2D normal_map;
layout(set = 1, binding = 13) uniform sampler2D metallic_map;
layout(set = 1, binding = 14) uniform sampler2D roughness_map;
layout(set = 1, binding = 15) uniform sampler2D ao_map;

#endif
