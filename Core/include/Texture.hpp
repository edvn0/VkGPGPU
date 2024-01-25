#pragma once

#include "DataBuffer.hpp"
#include "Device.hpp"
#include "Filesystem.hpp"
#include "Image.hpp"
#include "ImageProperties.hpp"

namespace Core {

enum class TextureDataStrategy : std::uint8_t { None, Keep, Delete };
enum class MipGenerationStrategy : std::uint8_t {
  FromSize,
  Literal,
  Unused,
};

struct MipGeneration {
  MipGenerationStrategy strategy{MipGenerationStrategy::Literal};
  u32 mips{1};
};

struct TextureProperties {
  ImageFormat format;
  std::string identifier{};
  FS::Path path{};
  Extent<u32> extent{};
  TextureDataStrategy texture_data_strategy{TextureDataStrategy::Delete};
  ImageTiling tiling{ImageTiling::Optimal};
  ImageUsage usage{ImageUsage::Sampled};
  ImageLayout layout{ImageLayout::ShaderReadOnlyOptimal};
  SamplerFilter min_filter{SamplerFilter::Nearest};
  SamplerFilter max_filter{SamplerFilter::Nearest};
  SamplerAddressMode address_mode{SamplerAddressMode::Repeat};
  SamplerBorderColor border_color{SamplerBorderColor::FloatOpaqueBlack};
  MipGeneration mip_generation{};
};

class Texture {
public:
  ~Texture();
  auto on_resize(const Extent<u32> &) -> void;

  [[nodiscard]] auto get_image_info() const noexcept
      -> const VkDescriptorImageInfo &;
  [[nodiscard]] auto get_image() const noexcept -> const Image &;
  [[nodiscard]] auto valid() const noexcept -> bool;

  auto write_to_file(const FS::Path &) -> bool;
  [[nodiscard]] auto size_bytes() const -> usize { return cached_size; }
  [[nodiscard]] auto get_extent() const -> const auto & {
    return properties.extent;
  }

  [[nodiscard]] auto hash() const -> usize;

  static auto empty_with_size(const Device &, usize, const Extent<u32> &)
      -> Scope<Texture>;
  static auto construct(const Device &, const TextureProperties &)
      -> Scope<Texture>;
  static auto construct_storage(const Device &, const TextureProperties &)
      -> Scope<Texture>;
  static auto construct_shader(const Device &, const TextureProperties &)
      -> Scope<Texture>;
  static auto construct(const Device &, const FS::Path &) -> Scope<Texture>;

private:
  Texture(const Device &, const TextureProperties &);
  Texture(const Device &, usize, const Extent<u32> &);

  const Device *device{nullptr};
  TextureProperties properties;

  DataBuffer data_buffer;
  usize cached_size{0};
  bool storage{false};

  Scope<Image> image{nullptr};
};

} // namespace Core
