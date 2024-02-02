#version 460

#include <ShaderResources.glsl>

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uvs;
layout(location = 2) in vec4 colour;
layout(location = 3) in vec3 normals;
layout(location = 4) in vec3 tangent;
layout(location = 5) in vec3 bitangents;

layout(location = 0) out vec3 near_point;
layout(location = 1) out vec3 far_point;

vec3 UnprojectPoint(float x, float y, float z, mat4 view, mat4 projection)
{
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(projection);
    vec4 unprojectedPoint = viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main()
{
    near_point =
        UnprojectPoint(pos.x, pos.y, 0.0, renderer.view, renderer.projection).xyz;
    far_point =
        UnprojectPoint(pos.x, pos.y, 1.0, renderer.view, renderer.projection).xyz;
    gl_Position = vec4(pos, 1.0);
}
