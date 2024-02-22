#include "Concepts.hpp"
#include "Containers.hpp"
#include "Types.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <entt/entt.hpp>
#include <functional>
#include <numeric>
#include <sstream>

#include "ecs/Entity.hpp"
#include "ecs/Scene.hpp"
#include "ecs/serialisation/SceneSerialiser.hpp" // Include the header where your SceneSerialiser is defined

static constexpr auto create_stream = []() -> std::stringstream {
  return std::stringstream{std::ios::binary | std::ios::out | std::ios::in};
};

template <typename K, typename V>
auto value_compare_two_maps(const std::unordered_map<K, V> &lhs,
                            const std::unordered_map<K, V> &rhs) -> bool {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (auto &&[key, value] : lhs) {
    if (!rhs.contains(key)) {
      return false;
    }
    if (rhs.at(key) != value) {
      return false;
    }
  }
  return true;
}

auto value_compare_two_containers(const Core::Container::Container auto &lhs,
                                  const Core::Container::Container auto &rhs)
    -> bool {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

TEST_CASE("Basic type serialisation and deserialition, with the read/write "
          "functions") {
  using namespace ECS;
  SECTION("Serialise and deserialise glm vec3") {
    glm::vec3 vec3{1.0f, 2.0f, 3.0f};
    std::stringstream stream = create_stream();
    write(stream, vec3);
    glm::vec3 newVec3;
    read(stream, newVec3);
    REQUIRE(newVec3 == vec3);
  }

  SECTION("Serialise and deserialise glm vec4") {
    glm::vec4 vec4{1.0f, 2.0f, 3.0f, 4.0f};
    std::stringstream stream = create_stream();
    write(stream, vec4);
    glm::vec4 newVec4;
    read(stream, newVec4);
    REQUIRE(newVec4 == vec4);
  }

  SECTION("Serialise and deserialise glm mat4") {
    glm::mat4 mat4{1.0f};
    std::stringstream stream = create_stream();
    write(stream, mat4);
    glm::mat4 newMat4;
    read(stream, newMat4);
    REQUIRE(newMat4 == mat4);
  }

  SECTION("Serialise and deserialise std::string") {
    std::string str{"Hello, World!"};
    std::stringstream stream = create_stream();
    write(stream, str);
    std::string newStr;
    read(stream, newStr);
    REQUIRE(newStr == str);
  }

  SECTION("Serialise and deserialise float") {
    float f = 1.0f;
    std::stringstream stream = create_stream();
    write(stream, f);
    float newF;
    read(stream, newF);
    REQUIRE(newF == f);
  }

  SECTION("Serialise and deserialise int") {
    int i = 1;
    std::stringstream stream = create_stream();
    write(stream, i);
    int newI;
    read(stream, newI);
    REQUIRE(newI == i);
  }

  SECTION("Serialise and deserialise unsigned int") {
    unsigned int i = 1;
    std::stringstream stream = create_stream();
    write(stream, i);
    unsigned int newI;
    read(stream, newI);
    REQUIRE(newI == i);
  }

  SECTION("Serialise and deserialise bool") {
    bool b = true;
    std::stringstream stream = create_stream();
    write(stream, b);
    bool newB;
    read(stream, newB);
    REQUIRE(newB == b);
  }

  SECTION("Serialise vector of glm vec3") {
    std::vector<glm::vec3> vec3s(10, glm::vec3(1.0f, 2.0f, 3.0f));
    std::stringstream stream = create_stream();
    write(stream, vec3s);
    std::vector<glm::vec3> newVec3s;
    read(stream, newVec3s);
    REQUIRE(newVec3s == vec3s);
  }

  SECTION("Serialise vector of strings ") {
    std::vector<std::string> strings(10, "Hello, World!");
    std::stringstream stream = create_stream();
    write(stream, strings);
    std::vector<std::string> newStrings;
    read(stream, newStrings);
    REQUIRE(newStrings == strings);
  }

  SECTION("Serialise vector of floats") {
    std::vector<float> floats(10, 1.0f);
    std::stringstream stream = create_stream();
    write(stream, floats);
    std::vector<float> newFloats;
    read(stream, newFloats);
    REQUIRE(newFloats == floats);
  }

  SECTION("Serialise vector of ints") {
    std::vector<int> ints(10, 1);
    std::stringstream stream = create_stream();
    write(stream, ints);
    std::vector<int> newInts;
    read(stream, newInts);
    REQUIRE(newInts == ints);
  }

  SECTION("Serialise vector of unsigned ints") {
    std::vector<unsigned int> ints(10, 1);
    std::stringstream stream = create_stream();
    write(stream, ints);
    std::vector<unsigned int> newInts;
    read(stream, newInts);
    REQUIRE(newInts == ints);
  }

  SECTION("Serialise std::unordered_map<std::integral, std::string>") {
    std::unordered_map<int, std::string> map;
    for (int i = 0; i < 10; i++) {
      map[i] = "Hello, World!";
    }

    std::stringstream stream = create_stream();
    write(stream, map);
    std::unordered_map<int, std::string> newMap;
    read(stream, newMap);
    for (auto &&[key, value] : map) {
      REQUIRE(newMap[key] == value);
    }
  }
}

TEST_CASE("Individual component serialisation", "[component]") {
  using namespace ECS;
  SECTION("Deser identifier component") {
    auto stream = create_stream();
    auto old_identity = IdentityComponent{"Entity1"};
    ComponentSerialiser<IdentityComponent>::serialise(old_identity, stream);
    IdentityComponent newIdentity;
    ComponentSerialiser<IdentityComponent>::deserialise(stream, newIdentity);
    REQUIRE(newIdentity.name == "Entity1");
    REQUIRE(newIdentity.id == old_identity.id);
  }

  SECTION("Deser transform component") {
    auto stream = create_stream();
    auto old_transform = TransformComponent{glm::vec3(1.0f, 2.0f, 3.0f)};
    ComponentSerialiser<TransformComponent>::serialise(old_transform, stream);
    TransformComponent newTransform;
    ComponentSerialiser<TransformComponent>::deserialise(stream, newTransform);
    REQUIRE(newTransform.position == glm::vec3(1.0f, 2.0f, 3.0f));
  }

  SECTION("Deser texture component") {
    auto stream = create_stream();
    auto old_texture = TextureComponent{glm::vec4{1.0f, 2.0f, 3.0f, 4.0f}};
    ComponentSerialiser<TextureComponent>::serialise(old_texture, stream);
    TextureComponent newTexture;
    ComponentSerialiser<TextureComponent>::deserialise(stream, newTexture);
    REQUIRE(newTexture.colour == glm::vec4{1.0f, 2.0f, 3.0f, 4.0f});
  }

  SECTION("Deser mesh component") {
    auto stream = create_stream();
    auto old_mesh = MeshComponent{nullptr, "Test"};
    ComponentSerialiser<MeshComponent>::serialise(old_mesh, stream);
    MeshComponent newMesh;
    ComponentSerialiser<MeshComponent>::deserialise(stream, newMesh);
    REQUIRE(newMesh.path == "Test");
  }

  SECTION("Deser camera component") {
    auto stream = create_stream();
    auto old_camera = CameraComponent{1.0f};
    ComponentSerialiser<CameraComponent>::serialise(old_camera, stream);
    CameraComponent newCamera;
    ComponentSerialiser<CameraComponent>::deserialise(stream, newCamera);
    REQUIRE(newCamera.field_of_view == 1.0f);
  }
}

TEST_CASE("Component Serialization and Deserialization", "[serialization]") {
  SECTION("Serialise and Deserialise Identifier Component") {
    ECS::Scene scene("Test");
    auto entity = scene.create_entity("Entity1");
    std::stringstream stream = create_stream();
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
    auto &old_transform = entity.add_component<ECS::TransformComponent>();
    old_transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    std::stringstream stream = create_stream();
    ECS::SceneSerialiser serialiser;
    serialiser.serialise(scene, stream);
    ECS::Scene newScene("Test");
    serialiser.deserialise(newScene, stream);
    auto view = newScene.get_registry().view<ECS::TransformComponent>();
    REQUIRE(view.size() == 1);
    for (auto e : view) {
      const auto &trans = view.get<ECS::TransformComponent>(e);
      REQUIRE(trans.position.x == Catch::Approx(old_transform.position.x));
      REQUIRE(trans.position.y == Catch::Approx(old_transform.position.y));
      REQUIRE(trans.position.z == Catch::Approx(old_transform.position.z));
    }
  }

  SECTION("Serialise and Deserialise Texture Component") {
    ECS::Scene scene("Test");
    auto entity = scene.create_entity("Entity1");
    entity.add_component<ECS::TextureComponent>(
        glm::vec4{1.0f, 2.0f, 3.0f, 4.0f});
    std::stringstream stream = create_stream();
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
    const auto copy = entity.add_component<ECS::MeshComponent>(nullptr, "Test");
    std::stringstream stream = create_stream();
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
    std::stringstream stream = create_stream();
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
    auto &old_transform = entity.add_component<ECS::TransformComponent>(
        glm::vec3(1.0f, 2.0f, 3.0f));
    entity.add_component<ECS::TextureComponent>(
        glm::vec4{1.0f, 2.0f, 3.0f, 4.0f});
    entity.add_component<ECS::MeshComponent>(nullptr, "Test");
    entity.add_component<ECS::CameraComponent>(1.0f);
    std::stringstream stream = create_stream();
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
      auto &trans = view.get<ECS::TransformComponent>(e);
      REQUIRE(trans.position.x == Catch::Approx(old_transform.position.x));
      REQUIRE(trans.position.y == Catch::Approx(old_transform.position.y));
      REQUIRE(trans.position.z == Catch::Approx(old_transform.position.z));
      REQUIRE(view.get<ECS::TextureComponent>(e).colour ==
              glm::vec4{1.0f, 2.0f, 3.0f, 4.0f});
      REQUIRE(view.get<ECS::MeshComponent>(e).path == "Test");
      REQUIRE(view.get<ECS::CameraComponent>(e).field_of_view == 1.0f);
    }
  }
}

struct Complex {
  float a;
  int b;
  std::string c;
  glm::vec3 d;

  auto operator==(const Complex &other) const -> bool {
    return a == other.a && b == other.b && c == other.c && d == other.d;
  }
};

auto write(std::ostream &out, const Complex &complex) -> bool {
  if (!ECS::write(out, complex.a))
    return false;
  if (!ECS::write(out, complex.b))
    return false;
  if (!ECS::write(out, complex.c))
    return false;
  if (!ECS::write(out, complex.d))
    return false;
  return true;
}

auto read(std::istream &in, Complex &complex) -> bool {
  if (!ECS::read(in, complex.a))
    return false;
  if (!ECS::read(in, complex.b))
    return false;
  if (!ECS::read(in, complex.c))
    return false;
  if (!ECS::read(in, complex.d))
    return false;
  return true;
}

TEST_CASE("Complex type std serialisation") {
  using namespace ECS;
  SECTION("Complex type serialisation for std::vector") {
    std::vector<Complex> vec;
    vec.reserve(10);
    for (int i = 0; i < 10; i++) {
      vec.push_back(
          Complex{1.0f, 2, "Hello, World!", glm::vec3(1.0f, 2.0f, 3.0f)});
    }
    std::stringstream stream = create_stream();
    write(stream, vec);
    std::vector<Complex> newVec;
    read(stream, newVec);
    REQUIRE(newVec.size() == vec.size());
    REQUIRE(value_compare_two_containers(vec, newVec));
    REQUIRE(newVec == vec);
  }

  SECTION("Complex type serialisation for std::unordered_map") {
    std::unordered_map<int, Complex> map;
    for (int i = 0; i < 10; i++) {
      map[i] = Complex{1.0f, 2, "Hello, World!", glm::vec3(1.0f, 2.0f, 3.0f)};
    }
    std::stringstream stream = create_stream();
    write(stream, map);
    std::unordered_map<int, Complex> newMap;
    read(stream, newMap);
    REQUIRE(value_compare_two_maps(map, newMap));
    for (auto &&[key, value] : map) {
      REQUIRE(newMap[key].a == value.a);
      REQUIRE(newMap[key].b == value.b);
      REQUIRE(newMap[key].c == value.c);
      REQUIRE(newMap[key].d == value.d);
    }
  }
}

TEST_CASE("Enum serialization and deserialization", "[enum]") {
  enum class ExampleEnum : int { First = 1, Second = 2, Third = 3 };
  SECTION("Serialise and deserialise ExampleEnum") {
    ExampleEnum originalValue = ExampleEnum::Second;
    std::stringstream stream = create_stream();

    bool writeSuccess = ECS::write(stream, originalValue);
    REQUIRE(writeSuccess);

    ExampleEnum deserializedValue;

    bool readSuccess = ECS::read(stream, deserializedValue);
    REQUIRE(readSuccess);

    REQUIRE(deserializedValue == originalValue);
  }

  SECTION("Validate deserialization of invalid ExampleEnum value") {
    int invalidValue = 100;
    std::stringstream stream = create_stream();

    ECS::write(stream, invalidValue);

    ExampleEnum deserializedValue = ExampleEnum::First;
    bool readSuccess = ECS::read(stream, deserializedValue);

    REQUIRE_FALSE(readSuccess);
  }
}

TEST_CASE("Enum serialization and deserialization") {
  enum class ExampleEnum : int { First = 1, Value2 = 9, Value3 = 100 };

  SECTION("Value2") {
    std::stringstream stream = create_stream();
    ExampleEnum originalValue = ExampleEnum::Value2;

    REQUIRE(ECS::write(stream, originalValue));

    ExampleEnum deserializedValue;
    REQUIRE(ECS::read(stream, deserializedValue));

    REQUIRE(deserializedValue == originalValue);
  }

  SECTION("First") {
    std::stringstream stream = create_stream();
    ExampleEnum originalValue = ExampleEnum::First;

    REQUIRE(ECS::write(stream, originalValue));

    ExampleEnum deserializedValue;
    REQUIRE(ECS::read(stream, deserializedValue));

    REQUIRE(deserializedValue == originalValue);
  }

  SECTION("Value3") {
    std::stringstream stream = create_stream();
    ExampleEnum originalValue = ExampleEnum::Value3;

    REQUIRE(ECS::write(stream, originalValue));

    ExampleEnum deserializedValue;
    REQUIRE(ECS::read(stream, deserializedValue));

    REQUIRE(deserializedValue == originalValue);
  }
}

TEST_CASE("GeometryComponent Serialization and Deserialization",
          "[serialization][GeometryComponent]") {
  using namespace ECS;

  SECTION("QuadParameters Serialization and Deserialization") {
    GeometryComponent originalComponent;
    originalComponent.parameters = BasicGeometry::QuadParameters{2.0f, 3.0f};

    std::stringstream stream = create_stream();
    ComponentSerialiser<GeometryComponent>::serialise(originalComponent,
                                                      stream);

    GeometryComponent deserializedComponent;
    ComponentSerialiser<GeometryComponent>::deserialise(stream,
                                                        deserializedComponent);

    auto &originalQuad =
        std::get<BasicGeometry::QuadParameters>(originalComponent.parameters);
    auto &deserializedQuad = std::get<BasicGeometry::QuadParameters>(
        deserializedComponent.parameters);

    REQUIRE(deserializedQuad.width == originalQuad.width);
    REQUIRE(deserializedQuad.height == originalQuad.height);
  }

  SECTION("TriangleParameters Serialization and Deserialization") {
    GeometryComponent originalComponent;
    originalComponent.parameters =
        BasicGeometry::TriangleParameters{4.0f, 5.0f};

    std::stringstream stream = create_stream();
    ComponentSerialiser<GeometryComponent>::serialise(originalComponent,
                                                      stream);

    GeometryComponent deserializedComponent;
    ComponentSerialiser<GeometryComponent>::deserialise(stream,
                                                        deserializedComponent);

    auto &originalTriangle = std::get<BasicGeometry::TriangleParameters>(
        originalComponent.parameters);
    auto &deserializedTriangle = std::get<BasicGeometry::TriangleParameters>(
        deserializedComponent.parameters);

    REQUIRE(deserializedTriangle.base == originalTriangle.base);
    REQUIRE(deserializedTriangle.height == originalTriangle.height);
  }

  // Repeat for CircleParameters, SphereParameters, and CubeParameters
}
