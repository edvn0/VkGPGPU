#version 460

layout(location = 0) out vec4 out_colour;

layout(set = 1, binding = 29) uniform samplerCube texture_cube;

layout(location = 0) in vec3 in_position;

void main() {
  out_colour = textureLod(texture_cube, in_position, 0.0F);
  out_colour.a = 1.0f;
}
