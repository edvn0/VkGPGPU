#pragma once

#include "Device.hpp"

namespace Core {

class TextureCube {
  struct TextureImageImpl;

public:
  auto get_descriptor_info() const -> const VkDescriptorImageInfo &;
  auto generate_mips(bool) -> void;

  static auto construct(const Device &, ImageFormat, const Extent<u32> &)
      -> Ref<TextureCube>;

private:
  TextureCube(const Device &device, ImageFormat, const Extent<u32> &);

  const Device *device{nullptr};
  ImageFormat format{ImageFormat::Undefined};
  Extent<u32> extent{};
  VkDescriptorImageInfo descriptor_info{};
  Scope<TextureImageImpl> impl{nullptr};

  bool mips_generated{false};

  auto create_empty_texture_cube() -> void;
  auto destroy() -> void;
};

} // namespace Core
