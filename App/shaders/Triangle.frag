#version 460

layout(set = 1, binding = 2) uniform sampler2D colour_texture;

layout(location = 0) in vec2 in_uvs;

layout(location = 0) out vec4 colour;

void main() {
  vec4 scene_colour = texture(colour_texture, in_uvs);
  colour = scene_colour;
}
