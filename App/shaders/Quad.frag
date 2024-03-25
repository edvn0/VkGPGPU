#version 460 core

layout(location = 0)in vec4 fragment_colour; // Received color from vertex shader
layout(location = 0)out vec4 out_colour; // Output color of the fragment

void main() {
    out_colour = fragment_colour; // Set the fragment's color
}
