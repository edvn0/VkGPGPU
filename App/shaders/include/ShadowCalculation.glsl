#ifndef GPGPU_SHADOW_CALCULATION
#define GPGPU_SHADOW_CALCULATION

// Performs Percentage-Closer Filtering around the shadow coordinate to achieve
// soft shadows or directly samples for hard shadows based on filter_size.
// Parameters:
// - shadowMap: The sampler2D containing the shadow map.
// - projection_coordinates: The projected coordinates in the shadow map.
// - bias: Depth bias to prevent shadow acne.
// - texelSize: The size of a single texel in the shadow map texture.
// - filter_size: The size of the filter kernel used for PCF. If filter_size is 0,
//   perform a single sample for hard shadows.
float shadowPCF(sampler2D shadowMap, vec3 projection_coordinates, float bias,
                vec2 texelSize, int filter_size) {
  if (filter_size == 0) {
    // Perform a single sample for hard shadows
    float pcf_depth = texture(shadowMap, projection_coordinates.xy).r;
    return (projection_coordinates.z - bias) < pcf_depth ? 1.0 : 0.0;
  } else {
    // Perform PCF for soft shadows
    float shadow = 0.0;
    int samples = 0;
    for (int x = -filter_size; x <= filter_size; ++x) {
      for (int y = -filter_size; y <= filter_size; ++y) {
        float pcf_depth =
            texture(shadowMap, projection_coordinates.xy + vec2(x, y) * texelSize).r;
        shadow += (projection_coordinates.z - bias) < pcf_depth ? 1.0 : 0.0;
        samples++;
      }
    }
    return shadow / float(samples);
  }
}

// Calculates the shadow factor using PCF for soft shadows or directly samples
// for hard shadows. This function needs to be adapted based on your shadow
// mapping setup, especially for converting from clip space to texture space
// and calculating the appropriate bias and texel size.
// Parameters:
// - shadowMap: The sampler2D containing the shadow map.
// - projection_coordinates: The normalized device coordinates (NDC) of the fragment.
// - normal: The normal vector at the fragment, used to calculate bias.
// - light_direction: The direction of the light, used to calculate bias.
// - shadowMapSize: The dimensions of the shadow map, used to calculate
// texelSize.
// - filter_size: The size of the filter kernel used for PCF. If filter_size is 0,
//   perform a single sample for hard shadows.
float calculateSoftShadow(sampler2D shadowMap, vec3 projection_coordinates, vec3 normal,
                          vec3 light_direction, vec2 shadowMapSize, int filter_size,
                          float bias_value) {
  float bias = max(0.05 * (1.0 - dot(normal, light_direction)), bias_value);
  vec2 texelSize = 1.0 / shadowMapSize;
  return shadowPCF(shadowMap, projection_coordinates, bias, texelSize, filter_size);
}

#endif // GPGPU_SHADOW_CALCULATION
