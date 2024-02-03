#version 460

#include <ShaderResources.glsl>

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 uvs;
layout(location = 2) in vec4 colour;
layout(location = 3) in vec3 normals;
layout(location = 4) in vec3 tangent;
layout(location = 5) in vec3 bitangents;

layout(location = 0) out vec4 fragPos; // Pass through for fragment shader

void main()
{
    // Define fullscreen quad vertices in NDC
    vec2 positions[4] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0),
                               vec2(1.0, 1.0));

    // Select vertex based on gl_VertexIndex
    vec2 position = positions[gl_VertexIndex];

    // Convert the fullscreen quad to cover entire screen
    vec4 clipPos = vec4(position, 0.0, 1.0);

    // Compute the inverse view and projection matrix
    mat4 invView = inverse(renderer.view);
    mat4 invProj = inverse(renderer.projection);

    // Convert clip space coordinates to world space
    vec4 worldPos = invView * invProj * clipPos;

    // For Z = 0 and Z = 1 (useful for grid calculation in fragment shader)
    fragPos = worldPos / worldPos.w;

    // Set gl_Position for the vertex
    gl_Position = clipPos;
}
