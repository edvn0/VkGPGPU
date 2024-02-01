#pragma once

#include "Logger.hpp"

#include <entt/entt.hpp>
#include <fstream>

#include "ecs/components/Component.hpp"

namespace ECS {

inline auto write(std::ostream &out, const entt::entity &entity) -> bool {
  if (!out.write(reinterpret_cast<const char *>(&entity), sizeof(entity)))
    return false;
  return true;
}

inline auto write(std::ostream &out, const std::integral auto &value) -> bool {
  if (!out.write(reinterpret_cast<const char *>(&value), sizeof(value)))
    return false;
  return true;
}

inline auto write(std::ostream &out, const std::floating_point auto &value)
    -> bool {
  if (!out.write(reinterpret_cast<const char *>(&value), sizeof(value)))
    return false;
  return true;
}

inline auto read(std::istream &in, entt::entity &entity) -> bool {
  if (!in.read(reinterpret_cast<char *>(&entity), sizeof(entity)))
    return false;
  return true;
}

inline auto read(std::istream &in, std::integral auto &value) -> bool {
  if (!in.read(reinterpret_cast<char *>(&value), sizeof(value)))
    return false;
  return true;
}

inline auto read(std::istream &in, std::floating_point auto &value) -> bool {
  if (!in.read(reinterpret_cast<char *>(&value), sizeof(value)))
    return false;
  return true;
}

inline auto write(std::ostream &out, const glm::vec2 &vec) -> bool {
  if (!write(out, vec[0]))
    return false;
  if (!write(out, vec[1]))
    return false;
  return true;
}

inline auto write(std::ostream &out, const glm::vec3 &vec) -> bool {
  if (!write(out, vec[0]))
    return false;
  if (!write(out, vec[1]))
    return false;
  if (!write(out, vec[2]))
    return false;
  return true;
}

inline auto write(std::ostream &out, const glm::vec4 &vec) -> bool {
  if (!write(out, vec[0]))
    return false;
  if (!write(out, vec[1]))
    return false;
  if (!write(out, vec[2]))
    return false;
  if (!write(out, vec[3]))
    return false;
  return true;
}

inline auto write(std::ostream &out, const glm::quat &quat) -> bool {
  if (!write(out, quat[0]))
    return false;
  if (!write(out, quat[1]))
    return false;
  if (!write(out, quat[2]))
    return false;
  if (!write(out, quat[3]))
    return false;
  return true;
}

inline auto read(std::istream &in, glm::vec2 &vec) -> bool {
  if (!read(in, vec[0]))
    return false;
  if (!read(in, vec[1]))
    return false;
  return true;
}

inline auto read(std::istream &in, glm::vec3 &vec) -> bool {
  if (!read(in, vec[0]))
    return false;
  if (!read(in, vec[1]))
    return false;
  if (!read(in, vec[2]))
    return false;
  return true;
}

inline auto read(std::istream &in, glm::vec4 &vec) -> bool {
  if (!read(in, vec[0]))
    return false;
  if (!read(in, vec[1]))
    return false;
  if (!read(in, vec[2]))
    return false;
  if (!read(in, vec[3]))
    return false;
  return true;
}

inline auto read(std::istream &in, glm::quat &quat) -> bool {
  if (!read(in, quat[0]))
    return false;
  if (!read(in, quat[1]))
    return false;
  if (!read(in, quat[2]))
    return false;
  if (!read(in, quat[3]))
    return false;
  return true;
}

inline auto write(std::ostream &out, const std::string &string) -> bool {
  const auto size = string.size();
  if (!out.write(reinterpret_cast<const char *>(&size), sizeof(size)))
    return false;
  if (!out.write(string.data(), size))
    return false;
  return true;
}

inline auto read(std::istream &in, std::string &string) -> bool {
  std::size_t size;
  if (!in.read(reinterpret_cast<char *>(&size), sizeof(size)))
    return false;
  string.resize(size);
  if (!in.read(string.data(), size))
    return false;
  return true;
}

template <class T> struct ComponentSerialiser;

template <> struct ComponentSerialiser<IdentityComponent> {
  static void serialise(const IdentityComponent &component,
                        std::ostream &out_stream) {
    write(out_stream, component.id);
    write(out_stream, component.name);
  }

  static IdentityComponent deserialise(std::istream &inFile) {
    IdentityComponent component;
    read(inFile, component.id);
    read(inFile, component.name);
    return component;
  }
};

template <> struct ComponentSerialiser<TransformComponent> {
  static void serialise(const TransformComponent &component,
                        std::ostream &out_stream) {
    if (write(out_stream, component.position)) {
    }
    if (write(out_stream, component.rotation)) {
    }
    if (write(out_stream, component.scale)) {
    }
  }

  static TransformComponent deserialise(std::istream &inFile) {
    TransformComponent component;
    if (read(inFile, component.position)) {
    }
    if (read(inFile, component.rotation)) {
    }
    if (read(inFile, component.scale)) {
    }

    return component;
  }
};

template <> struct ComponentSerialiser<TextureComponent> {
  static void serialise(const TextureComponent &component,
                        std::ostream &out_stream) {
    write(out_stream, component.colour);
  }

  static TextureComponent deserialise(std::istream &inFile) {
    TextureComponent component;
    read(inFile, component.colour);
    return component;
  }
};

template <> struct ComponentSerialiser<MeshComponent> {
  static void serialise(const MeshComponent &component,
                        std::ostream &out_stream) {
    // we only write the path to the mesh file, not the path field of the
    // component need to indicate to deserialiser if the mesh is loaded or not
    // (nullptr or not)
    write(out_stream, component.mesh != nullptr ? 1 : 0);
    if (component.mesh != nullptr) {
      write(out_stream, component.mesh->get_file_path().string());
    }
  }

  static MeshComponent deserialise(std::istream &inFile) {
    MeshComponent component;
    int is_loaded;
    read(inFile, is_loaded);
    if (is_loaded) {
      std::string path;
      read(inFile, path);
      component.path = path;
    }
    return component;
  }
};

template <> struct ComponentSerialiser<CameraComponent> {
  static void serialise(const CameraComponent &component,
                        std::ostream &out_stream) {
    write(out_stream, component.field_of_view);
  }

  static CameraComponent deserialise(std::istream &inFile) {
    CameraComponent component;
    read(inFile, component.field_of_view);
    return component;
  }
};

} // namespace ECS
