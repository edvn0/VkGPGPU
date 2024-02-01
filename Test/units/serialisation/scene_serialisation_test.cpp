#include "Types.hpp"

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include <functional>
#include <numeric>
#include <sstream>

#include "ecs/Entity.hpp"
#include "ecs/Scene.hpp"
#include "ecs/serialisation/SceneSerialiser.hpp" // Include the header where your SceneSerialiser is defined

TEST_CASE("Component Serialization and Deserialization", "[serialization]") {
  SECTION("Serialise and Deserialise Identifier Component") {
    ECS::Scene scene("Test");
    auto entity = scene.create_entity("Entity1");
    std::stringstream stream{std::ios::binary | std::ios::out | std::ios::in};
    ECS::SceneSerialiser serialiser;
    serialiser.serialise(scene, stream);
    ECS::Scene newScene("Test");
    serialiser.deserialise(newScene, stream);
    auto view = newScene.get_registry().view<ECS::IdentityComponent>();
    REQUIRE(view.size() == 1);
    for (auto e : view) {
      REQUIRE(view.get<ECS::IdentityComponent>(e).name == "Entity1");
    }
  }

  SECTION("Serialise and Deserialise Transform Component") {
    ECS::Scene scene("Test");
    auto entity = scene.create_entity("Entity1");
    entity.add_component<ECS::TransformComponent>(glm::vec3(1.0f, 2.0f, 3.0f));
    std::stringstream stream{std::ios::binary | std::ios::out | std::ios::in};
    ECS::SceneSerialiser serialiser;
    serialiser.serialise(scene, stream);
    ECS::Scene newScene("Test");
    serialiser.deserialise(newScene, stream);
    auto view = newScene.get_registry().view<ECS::TransformComponent>();
    REQUIRE(view.size() == 1);
    for (auto e : view) {
      const auto &trans = view.get<ECS::TransformComponent>(e);
      REQUIRE(trans.position == glm::vec3(1.0f, 2.0f, 3.0f));
    }
  }

  SECTION("Serialise and Deserialise Texture Component") {
    ECS::Scene scene("Test");
    auto entity = scene.create_entity("Entity1");
    entity.add_component<ECS::TextureComponent>(
        glm::vec4{1.0f, 2.0f, 3.0f, 4.0f});
    std::stringstream stream{std::ios::binary | std::ios::out | std::ios::in};
    ECS::SceneSerialiser serialiser;
    serialiser.serialise(scene, stream);
    ECS::Scene newScene("Test");
    serialiser.deserialise(newScene, stream);
    auto view = newScene.get_registry().view<ECS::TextureComponent>();
    REQUIRE(view.size() == 1);
    for (auto e : view) {
      REQUIRE(view.get<ECS::TextureComponent>(e).colour ==
              glm::vec4{1.0f, 2.0f, 3.0f, 4.0f});
    }
  }

  SECTION("Serialise and Deserialise Mesh Component") {
    ECS::Scene scene("Test");
    auto entity = scene.create_entity("Entity1");
    entity.add_component<ECS::MeshComponent>(nullptr, "Test");
    std::stringstream stream{std::ios::binary | std::ios::out | std::ios::in};
    ECS::SceneSerialiser serialiser;
    serialiser.serialise(scene, stream);
    ECS::Scene newScene("Test");
    serialiser.deserialise(newScene, stream);
    auto view = newScene.get_registry().view<ECS::MeshComponent>();
    REQUIRE(view.size() == 1);
    for (auto e : view) {
      REQUIRE(view.get<ECS::MeshComponent>(e).path == "Test");
    }
  }

  SECTION("Serialise and Deserialise Camera Component") {
    ECS::Scene scene("Test");
    auto entity = scene.create_entity("Entity1");
    entity.add_component<ECS::CameraComponent>(1.0f);
    std::stringstream stream{std::ios::binary | std::ios::out | std::ios::in};
    ECS::SceneSerialiser serialiser;
    serialiser.serialise(scene, stream);
    ECS::Scene newScene("Test");
    serialiser.deserialise(newScene, stream);
    auto view = newScene.get_registry().view<ECS::CameraComponent>();
    REQUIRE(view.size() == 1);
    for (auto e : view) {
      REQUIRE(view.get<ECS::CameraComponent>(e).field_of_view == 1.0f);
    }
  }

  SECTION("Serialise and Deserialise Multiple Components") {
    ECS::Scene scene("Test");
    auto entity = scene.create_entity("Entity1");
    entity.add_component<ECS::TransformComponent>(glm::vec3(1.0f, 2.0f, 3.0f));
    entity.add_component<ECS::TextureComponent>(
        glm::vec4{1.0f, 2.0f, 3.0f, 4.0f});
    entity.add_component<ECS::MeshComponent>(nullptr, "Test");
    entity.add_component<ECS::CameraComponent>(1.0f);
    std::stringstream stream{std::ios::binary | std::ios::out | std::ios::in};
    ECS::SceneSerialiser serialiser;
    serialiser.serialise(scene, stream);
    ECS::Scene newScene("Test");
    serialiser.deserialise(newScene, stream);
    auto view = newScene.get_registry()
                    .view<ECS::IdentityComponent, ECS::TransformComponent,
                          ECS::TextureComponent, ECS::MeshComponent,
                          ECS::CameraComponent>();
    REQUIRE(view.size_hint() == 1);
    for (auto e : view) {
      REQUIRE(view.get<ECS::TransformComponent>(e).position ==
              glm::vec3(1.0f, 2.0f, 3.0f));
      REQUIRE(view.get<ECS::TextureComponent>(e).colour ==
              glm::vec4{1.0f, 2.0f, 3.0f, 4.0f});
      REQUIRE(view.get<ECS::MeshComponent>(e).path == "Test");
      REQUIRE(view.get<ECS::CameraComponent>(e).field_of_view == 1.0f);
    }
  }
}
