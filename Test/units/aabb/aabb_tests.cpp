#include "AABB.hpp"

#include <catch2/catch_test_macros.hpp>

#include "catch2/catch_approx.hpp"

auto compare_two_glm_vectors(const glm::vec3 &lhs, const glm::vec3 &rhs)
    -> bool {
  return lhs.x == Catch::Approx(rhs.x) && lhs.y == Catch::Approx(rhs.y) &&
         lhs.z == Catch::Approx(rhs.z);
}

TEST_CASE("AABB Initialization", "[AABB]") {
  Core::AABB aabb(glm::vec2(0, 10), glm::vec2(0, 10), glm::vec2(0, 10));

  REQUIRE(compare_two_glm_vectors(aabb.min(), glm::vec3(0, 0, 0)));
  REQUIRE(compare_two_glm_vectors(aabb.max(), glm::vec3(10, 10, 10)));
}

TEST_CASE("AABB Update with Vertex", "[AABB]") {
  Core::AABB aabb;
  aabb.update(glm::vec3(5, 5, 5)); // Update with a single point

  // REQUIRE(aabb.min() == glm::vec3(5, 5, 5));
  // REQUIRE(aabb.max() == glm::vec3(5, 5, 5));
  REQUIRE(compare_two_glm_vectors(aabb.min(), glm::vec3(5, 5, 5)));
  REQUIRE(compare_two_glm_vectors(aabb.max(), glm::vec3(5, 5, 5)));

  aabb.update(glm::vec3(-5, -5, -5)); // Expand the AABB
  aabb.update(glm::vec3(10, 10, 10)); // Expand the AABB further

  REQUIRE(compare_two_glm_vectors(aabb.min(), glm::vec3(-5, -5, -5)));
  REQUIRE(compare_two_glm_vectors(aabb.max(), glm::vec3(10, 10, 10)));
}

TEST_CASE("AABB Update with Min and Max Vectors", "[AABB]") {
  Core::AABB aabb;
  aabb.update(glm::vec3(-10, -10, -10), glm::vec3(10, 10, 10));

  REQUIRE(compare_two_glm_vectors(aabb.min(), glm::vec3(-10, -10, -10)));
  REQUIRE(compare_two_glm_vectors(aabb.max(), glm::vec3(10, 10, 10)));
}

TEST_CASE("AABB Expansion and Contraction", "[AABB]") {
  Core::AABB aabb(glm::vec2(-10, 10), glm::vec2(-10, 10), glm::vec2(-10, 10));
  aabb.update(glm::vec3(-20, -20, -20), glm::vec3(20, 20, 20)); // Should expand

  REQUIRE(aabb.min() == glm::vec3(-20, -20, -20));
  REQUIRE(aabb.max() == glm::vec3(20, 20, 20));

  // Trying to "contract" should have no effect, as the AABB is already
  // encompassing a larger space
  aabb.update(glm::vec3(-5, -5, -5), glm::vec3(5, 5, 5));

  REQUIRE(aabb.min() == glm::vec3(-20, -20, -20));
  REQUIRE(aabb.max() == glm::vec3(20, 20, 20));
}
