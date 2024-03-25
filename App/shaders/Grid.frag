#version 460

// Taken verbatim (minus my uniforms)
// from https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/

#include <ShaderResources.glsl>

layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;
layout(location = 0) out vec4 outColor;

vec4 calc_grid(vec3 fragPos3D, float scale, bool drawAxis) {
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 g = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(g.x, g.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    vec3 grid_colour = grid.grid_colour.xyz;
    vec4 color = vec4(grid_colour, 1.0 - min(line, 1.0));
    // z axis
    if (drawAxis && fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx){
      color.z = 1.0; }
    // x axis
    if (drawAxis && fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz){
      color.x = 1.0; }
    return color;
}
float computeDepth(vec3 pos) {
    vec4 clip_space_position = renderer.projection * renderer.view * vec4 (pos.xyz, 1.0);
    return 0.5 + 0.5 * (clip_space_position.z / clip_space_position.w);
}
float computeLinearDepth(vec3 pos) {
    vec4 clip_space_pos = renderer.projection * renderer.view * vec4(pos.xyz, 1.0);
    float near = grid.grid_size.z;
    float far = grid.grid_size.w;
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0;// put back between -1 and 1
    float linearDepth = (2.0 * near * far) / (far + near - clip_space_depth * (far - near));// get linear value between 0.01 and 100
    return linearDepth / far;// normalize
}
void main() {
    float t = -nearPoint.y / (farPoint.y - nearPoint.y);
    vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);

    gl_FragDepth = computeDepth(fragPos3D);

    float linearDepth = computeLinearDepth(fragPos3D);
    float fading = max(0, (0.5 - linearDepth));

    outColor = (calc_grid(fragPos3D, grid.grid_size.x, true) + calc_grid(fragPos3D, grid.grid_size.y, true))* float(t > 0);// adding multiple resolution for the grid
    outColor.a *= fading;

    // Conditionally set gl_FragDepth for grid fragments
    // For the grid, we want to ensure it's rendered at the farthest depth
    // Assuming the rest of your scene's depth is handled correctly, and this shader is specifically for the grid
    // gl_FragDepth = comn;
    gl_FragDepth = 0.0;
}
