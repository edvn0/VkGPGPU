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

layout(set = 1, binding = 0, rgba8) readonly uniform image2D input_image;
layout(set = 1, binding = 1, rgba8) writeonly uniform image2D output_image;

layout(location = 0) out vec2 out_uvs;

void main() {
  out_uvs = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(out_uvs * 2.0f + -1.0f, 0.0f, 1.0f);
}
