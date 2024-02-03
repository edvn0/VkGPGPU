#version 460

// Taken verbatim (minus my uniforms)
// from https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/

#include <ShaderResources.glsl>

layout(location = 0) in vec4 fragPos;   // Received from vertex shader
layout(location = 0) out vec4 outColor; // Output color of the fragment

void main()
{
  float nearPlane = grid.grid_size.z;
  float farPlane = grid.grid_size.w;

  // Calculate depth for depth-based effects
  float depth = (fragPos.z - nearPlane) / (farPlane - nearPlane);
  depth = clamp(depth, 0.0, 1.0); // Ensure depth is within [0, 1]

  // Adjust grid spacing based on depth to simulate perspective
  float adjustedGridSpacing =
      grid.grid_size.x * mix(1.0, 2.0, depth); // Example adjustment

  // Calculate grid lines using modulo operation on x and z
  vec2 gridPosition = fragPos.xz / adjustedGridSpacing;
  vec2 gridMod = abs(mod(gridPosition, 1.0) - 0.5);
  float gridVisibility = smoothstep(0.05, 0.1, min(gridMod.x, gridMod.y));

  // Apply fade based on depth
  float fadeFactor = 1.0 - (depth);
  fadeFactor = clamp(fadeFactor, 0.0, 1.0); // Ensure factor is within [0, 1]

  // Combine grid visibility and fade factor
  float visibility = gridVisibility * fadeFactor;

  // Set grid color and apply combined visibility
  vec3 gridColor = vec3(1.0); // White grid
  outColor = vec4(gridColor * 1.0, 1.0);

  // Additional visual effects (like blur) could be applied based on depth
}
