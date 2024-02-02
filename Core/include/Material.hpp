#pragma once

#include "Buffer.hpp"
#include "Concepts.hpp"
#include "Config.hpp"
#include "Device.hpp"
#include "Math.hpp"
#include "Texture.hpp"

#include <BufferSet.hpp>
#include <optional>

#include "reflection/ReflectionData.hpp"

namespace Core {

class Material {
public:
  static auto construct(const Device &, const Shader &) -> Scope<Material>;
  static auto construct_reference(const Device &, const Shader &)
      -> Ref<Material>;

  auto on_resize(const Extent<u32> &) -> void;

  auto set(const std::string_view identifier, const IsBuiltin auto &value)
      -> bool {
    const auto &copy = value;
    return set(identifier, static_cast<const void *>(&copy));
  }

  auto set(const std::string_view identifier, Math::IsGLM auto &value) -> bool {
    const auto &copy = value;
    return set(identifier, Math::value_ptr(copy));
  }

  auto set(std::string_view, const Texture &) -> bool;
  auto set(std::string_view, const Image &) -> bool;
  auto set(std::string_view, const Buffer &) -> bool;
  [[nodiscard]] auto get_constant_buffer() const -> const auto & {
    return constant_buffer;
  }

  auto
  update_for_rendering(FrameIndex frame_index,
                       const std::vector<std::vector<VkWriteDescriptorSet>> &)
      -> void;
  auto update_for_rendering(FrameIndex frame_index) -> void {
    update_for_rendering(frame_index, {});
  }

  template <class T>
    requires requires(const T &t) {
      { t.get_pipeline_layout() } -> std::same_as<const VkPipelineLayout &>;
      { t.get_bind_point() } -> std::same_as<const VkPipelineBindPoint &>;
    }
  auto bind(const CommandBuffer &command_buffer, const T &pipeline, u32 frame,
            VkDescriptorSet renderer_set = nullptr) const -> void {
    bind_impl(command_buffer, pipeline.get_pipeline_layout(),
              pipeline.get_bind_point(), frame, renderer_set);
  }

  [[nodiscard]] auto get_shader() const -> const auto & { return *shader; }

private:
  Material(const Device &, const Shader &);
  auto construct_buffers() -> void;

  auto bind_impl(const CommandBuffer &, const VkPipelineLayout &,
                 const VkPipelineBindPoint &, u32 frame,
                 VkDescriptorSet additional_set) const -> void;

  auto set(std::string_view, const void *data) -> bool;
  [[nodiscard]] auto find_resource(std::string_view)
      -> std::optional<const Reflection::ShaderResourceDeclaration *>;
  [[nodiscard]] auto find_uniform(std::string_view) const
      -> std::optional<const Reflection::ShaderUniform *>;

  const Device *device{nullptr};
  const Shader *shader{nullptr};

  DataBuffer constant_buffer{};
  void initialise_constant_buffer();

  auto invalidate_descriptor_sets() -> void;
  auto invalidate() -> void;

  enum class PendingDescriptorType : std::uint8_t {
    None = 0,
    Texture2D = 1,
    TextureCube = 2,
    Image2D = 3,
  };

  struct PendingDescriptor {
    PendingDescriptorType type = PendingDescriptorType::None;
    VkWriteDescriptorSet write_set{};
    VkDescriptorImageInfo image_info{};
    const Texture *texture{nullptr};
    const Image *image{nullptr};
    VkDescriptorImageInfo descriptor_image_info{};
  };

  struct PendingDescriptorArray {
    PendingDescriptorType type = PendingDescriptorType::None;
    VkWriteDescriptorSet write_set;
    std::vector<VkDescriptorImageInfo> image_infos;
    std::vector<Ref<Texture>> textures;
    std::vector<Ref<Image>> images;
    VkDescriptorImageInfo descriptor_image_info{};
  };
  std::unordered_map<uint32_t, std::shared_ptr<PendingDescriptor>>
      resident_descriptors;
  std::unordered_map<uint32_t, std::shared_ptr<PendingDescriptorArray>>
      resident_descriptor_arrays;
  std::vector<std::shared_ptr<PendingDescriptor>> pending_descriptors;

  DataBuffer uniform_storage_buffer;

  std::unordered_map<FrameIndex, Reflection::MaterialDescriptorSet>
      descriptor_sets{};

  std::vector<const Texture *> texture_references;
  std::vector<const Image *> image_references;

  std::vector<std::vector<VkWriteDescriptorSet>> write_descriptors;
  std::vector<bool> dirty_descriptor_sets;

  std::unordered_map<std::string_view, Reflection::ShaderResourceDeclaration>
      identifiers{};
};

} // namespace Core
