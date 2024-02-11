#version 460

#include <ShaderResources.glsl>

layout(location = 0) in vec3 in_position; // Vertex position
layout(location = 1) in vec4 in_colour; // Vertex color

layout(location = 0 )out vec4 out_colour; // Pass color to fragment shader

void main() {
    out_colour = in_colour; // Pass color to fragment shader
    gl_Position = renderer.view_projection * vec4(in_position, 1.0); // Transform vertex position
}
