#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <random>

namespace Core {

class Random {
public:
  // Returns a random glm::vec3 where each component is in the range [min, max]
  static glm::vec3 vec3(float min, float max) {
    std::uniform_real_distribution<float> dis(min, max);
    return glm::vec3(dis(gen), dis(gen), dis(gen));
  }

  // Returns a random point on the surface of a sphere with a given radius
  static glm::vec3 on_sphere_surface(float radius) {
    std::uniform_real_distribution<float> dis(0.0F, 1.0F);

    float phi = dis(gen) * 2.0F * glm::pi<float>();
    float costheta = dis(gen) * 2.0F - 1.0F;

    float theta = acos(costheta);

    float sinTheta = sin(theta);
    float cosTheta = cos(theta);
    float sinPhi = sin(phi);
    float cosPhi = cos(phi);

    return glm::vec3(radius * sinTheta * cosPhi, radius * sinTheta * sinPhi,
                     radius * cosTheta);
  }

private:
  static inline std::random_device rd{};
  static inline std::mt19937 gen{rd()};
};

} // namespace Core
