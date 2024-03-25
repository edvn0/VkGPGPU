#version 460

#include <ShaderResources.glsl>

layout(location = 0) out vec3 eyeDirection;

const vec2 quadVertices[6] =
    vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0), vec2(-1.0, 3.0),
           vec2(3.0, -1.0), vec2(3.0, 3.0));

void main() {
  vec2 position = quadVertices[gl_VertexIndex % 6];
  gl_Position = vec4(position, 1.0, 1.0);

  // Adjust for inverse Z-buffer
  gl_Position.z = gl_Position.w * 0.999999;

  // Unproject vertex positions to world space
  mat3 inverse_model_view = transpose(mat3(renderer.view));
  vec3 unprojected = (renderer.inverse_projection * gl_Position).xyz;
  eyeDirection = normalize(inverse_model_view * unprojected);
}
