#ifndef GPGPU_SHADOW_CALCULATION
#define GPGPU_SHADOW_CALCULATION

// Performs Percentage-Closer Filtering around the shadow coordinate to achieve
// soft shadows or directly samples for hard shadows based on filterSize.
// Parameters:
// - shadowMap: The sampler2D containing the shadow map.
// - projCoords: The projected coordinates in the shadow map.
// - bias: Depth bias to prevent shadow acne.
// - texelSize: The size of a single texel in the shadow map texture.
// - filterSize: The size of the filter kernel used for PCF. If filterSize is 0,
//   perform a single sample for hard shadows.
float shadowPCF(sampler2D shadowMap, vec3 projCoords, float bias,
                vec2 texelSize, int filterSize) {
  if (filterSize == 0) {
    // Perform a single sample for hard shadows
    float pcfDepth = texture(shadowMap, projCoords.xy).r;
    return (projCoords.z - bias) < pcfDepth ? 1.0 : 0.0;
  } else {
    // Perform PCF for soft shadows
    float shadow = 0.0;
    int samples = 0;
    for (int x = -filterSize; x <= filterSize; ++x) {
      for (int y = -filterSize; y <= filterSize; ++y) {
        float pcfDepth =
            texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
        shadow += (projCoords.z - bias) < pcfDepth ? 1.0 : 0.0;
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
// - projCoords: The normalized device coordinates (NDC) of the fragment.
// - normal: The normal vector at the fragment, used to calculate bias.
// - lightDir: The direction of the light, used to calculate bias.
// - shadowMapSize: The dimensions of the shadow map, used to calculate
// texelSize.
// - filterSize: The size of the filter kernel used for PCF. If filterSize is 0,
//   perform a single sample for hard shadows.
float calculateSoftShadow(sampler2D shadowMap, vec3 projCoords, vec3 normal,
                          vec3 lightDir, vec2 shadowMapSize, int filterSize,
                          float bias_value) {
  float bias = max(0.05 * (1.0 - dot(normal, lightDir)), bias_value);
  vec2 texelSize = 1.0 / shadowMapSize;
  return shadowPCF(shadowMap, projCoords, bias, texelSize, filterSize);
}

#endif // GPGPU_SHADOW_CALCULATION
