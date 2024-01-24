#version 460

layout(location = 0) in vec2 in_uvs;

layout(location = 0) out vec4 colour;

layout(set = 1, binding = 0) uniform sampler2D geometry_texture;

void main() {
  vec4 scene_colour = texture(geometry_texture, in_uvs);

  colour = scene_colour;
  if (colour.a < 0.1)
    discard;
}
