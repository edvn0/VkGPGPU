#version 460

layout(push_constant) uniform PushConstants {
  int kernelSize;
  int precomputedCenterValue;
  int halfSize;
}
pc;

struct Mat4 {
  mat4 matrix;
};

layout(std140, set = 0, binding = 0) readonly buffer InputBufferA {
  Mat4 inputMatrix[];
}
input_buffer_a;

layout(std140, set = 0, binding = 1) readonly buffer InputBufferB {
  Mat4 inputMatrix[];
}
input_buffer_b;

layout(std140, set = 0, binding = 2) writeonly buffer OutputBufferA {
  Mat4 outputMatrix[];
}
output_buffer_a;

layout(std140, set = 0, binding = 3) uniform UniformBuffer { float angle; }
ubo;

layout(set = 1, binding = 2) uniform sampler2D geometry_texture;

layout(location = 0) in vec2 in_uvs;

layout(location = 0) out vec4 colour;

void main() {
  vec4 scene_colour = texture(geometry_texture, in_uvs);

  colour = vec4(in_uvs.x, in_uvs.y, 0.0, 1.0);
}
