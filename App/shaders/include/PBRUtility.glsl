#ifndef GPGU_PBR_UTILITY
#define GPGU_PBR_UTILITY

// Define the value of PI, a fundamental constant used in various calculations.
const float PI = 3.141592653589793;

// Calculate Fresnel-Schlick approximation with roughness for reflectance.
vec3 fresnel_schlick_roughness(vec3 f0, float cos_theta, float roughness) {
  // f0: Reflectivity at normal incidence
  // cos_theta: Cosine of the angle between the normal and the view direction
  // roughness: Surface roughness factor
  return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - cos_theta, 5.0);
}

// Rotate a vector about the Y axis by a given angle.
vec3 rotate_vector_about_y(float angle, vec3 v) {
  // angle: The angle of rotation in radians
  // v: The original vector to be rotated
  float c = cos(angle);
  float s = sin(angle);
  return vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

// Compute the Fresnel-Schlick approximation without roughness.
vec3 fresnel_schlick(float cos_theta, vec3 f0) {
  // cos_theta: Cosine of the angle between the normal and the view direction
  // f0: Reflectivity at normal incidence
  return f0 + (1.0 - f0) * pow(1.0 - cos_theta, 5.0);
}

// Calculate the GGX distribution of normals.
float distribution_ggx(vec3 n, vec3 h, float roughness) {
  // n: Surface normal
  // h: Halfway vector between view direction and light direction
  // roughness: Surface roughness factor
  float a = roughness * roughness;
  float a2 = a * a;
  float n_dot_h = max(dot(n, h), 0.0);
  float n_dot_h2 = n_dot_h * n_dot_h;

  float nom = a2;
  float denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return nom / denom;
}

// Calculate geometry function using Schlick-GGX approximation.
float geometry_schlick_ggx(float n_dot_v, float roughness) {
  // n_dot_v: Cosine of the angle between the normal and the view direction
  // roughness: Surface roughness factor
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float nom = n_dot_v;
  float denom = n_dot_v * (1.0 - k) + k;

  return nom / denom;
}

// Combine the geometry functions for the view and light directions using
// Smith's method.
float geometry_smith(vec3 n, vec3 v, vec3 l, float roughness) {
  // n: Surface normal
  // v: View direction
  // l: Light direction
  // roughness: Surface roughness factor
  float n_dot_v = max(dot(n, v), 0.0);
  float n_dot_l = max(dot(n, l), 0.0);
  float ggx2 = geometry_schlick_ggx(n_dot_v, roughness);
  float ggx1 = geometry_schlick_ggx(n_dot_l, roughness);

  return ggx1 * ggx2;
}

// Calculate image-based lighting (IBL) contribution.
vec3 ibl(samplerCube env_irradiance_tex, samplerCube env_radiance_tex,
         sampler2D brdf_lut, vec3 normal, vec3 view_dir, vec3 f0, vec3 albedo,
         float roughness, float metalness, float env_map_rotation, vec3 lr) {
  // env_irradiance_tex: Irradiance map
  // env_radiance_tex: Radiance map
  // brdf_lut: Look-up table for BRDF
  // normal: Surface normal
  // view_dir: View direction
  // f0: Reflectivity at normal incidence
  // albedo: Surface albedo
  // roughness: Surface roughness factor
  // metalness: Metalness factor
  // env_map_rotation: Rotation of the environment map
  // lr: Reflection direction for specular IBL
  vec3 irradiance = texture(env_irradiance_tex, normal).rgb;
  float n_dot_v = max(dot(normal, view_dir), 0.0);
  vec3 f = fresnel_schlick_roughness(f0, n_dot_v, roughness);
  vec3 kd = (1.0 - f) * (1.0 - metalness);
  vec3 diffuse_ibl = albedo * irradiance;

  int env_radiance_tex_levels = textureQueryLevels(env_radiance_tex);
  vec3 r = 2.0 * dot(view_dir, normal) * normal - view_dir;
  vec3 specular_irradiance =
      textureLod(env_radiance_tex, rotate_vector_about_y(env_map_rotation, lr),
                 roughness * float(env_radiance_tex_levels))
          .rgb;

  vec2 specular_brdf = texture(brdf_lut, vec2(n_dot_v, 1.0 - roughness)).rg;
  vec3 specular_ibl =
      specular_irradiance * (f0 * specular_brdf.x + specular_brdf.y);

  return kd * diffuse_ibl + specular_ibl;
}

#endif // GPGU_PBR_UTILITY
