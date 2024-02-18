#pragma once

#include "DataBuffer.hpp"
#include "Device.hpp"
#include "Filesystem.hpp"
#include "Image.hpp"
#include "ImageProperties.hpp"

#include <variant>

namespace Core {

enum class TextureDataStrategy : u8 { None, Keep, Delete };
enum class MipGenerationStrategy : u8 {
  FromSize,
  Literal,
  Unused,
};

struct LiteralMipData {
  u32 mips;
};

struct MipGeneration {
  std::variant<MipGenerationStrategy, LiteralMipData> strategy;

  MipGeneration() : strategy(MipGenerationStrategy::FromSize) {}

  explicit MipGeneration(MipGenerationStrategy strategy) : strategy(strategy) {}
  explicit MipGeneration(u32 mips) : strategy(LiteralMipData{mips}) {}

  MipGeneration(MipGenerationStrategy strat, u32 mips) {
    if (strat == MipGenerationStrategy::Literal) {
      strategy = LiteralMipData{mips};
    } else {
      strategy = strat;
    }
  }
};

struct TextureProperties {
  ImageFormat format;
  std::string identifier{};
  FS::Path path{};
  Extent<u32> extent{};
  TextureDataStrategy texture_data_strategy{TextureDataStrategy::Delete};
  ImageTiling tiling{ImageTiling::Optimal};
  ImageUsage usage{ImageUsage::Sampled | ImageUsage::TransferDst |
                   ImageUsage::TransferSrc};
  ImageLayout layout{ImageLayout::ShaderReadOnlyOptimal};
  SamplerFilter min_filter{SamplerFilter::Linear};
  SamplerFilter max_filter{SamplerFilter::Linear};
  SamplerAddressMode address_mode{SamplerAddressMode::Repeat};
  SamplerBorderColor border_color{SamplerBorderColor::FloatOpaqueBlack};
  MipGeneration mip_generation{MipGenerationStrategy::FromSize};
};

auto determine_mip_count(const MipGeneration &mipGeneration,
                         const Extent<u32> &extent) -> u32;
auto calculate_mip_count(const Extent<u32> &extent) -> u32;

class Texture {
public:
  ~Texture();
  auto on_resize(const Extent<u32> &) -> void;

  [[nodiscard]] auto get_image_info() const noexcept
      -> const VkDescriptorImageInfo &;
  [[nodiscard]] auto get_image() const noexcept -> const Image &;
  [[nodiscard]] auto valid() const noexcept -> bool;

  [[nodiscard]] auto write_to_file(const FS::Path &) const -> bool;
  [[nodiscard]] auto size_bytes() const -> usize { return cached_size; }
  [[nodiscard]] auto get_extent() const -> const auto & {
    return properties.extent;
  }

  [[nodiscard]] auto hash() const -> usize;
  [[nodiscard]] auto get_file_path() const
      -> std::optional<std::filesystem::path> {

    if (properties.path.empty()) {
      return std::nullopt;
    }

    return properties.path;
  }

  static auto empty_with_size(const Device &, usize, const Extent<u32> &)
      -> Scope<Texture>;
  static auto construct(const Device &, const TextureProperties &)
      -> Scope<Texture>;
  static auto construct_storage(const Device &, const TextureProperties &)
      -> Scope<Texture>;
  static auto construct_shader(const Device &, const TextureProperties &)
      -> Scope<Texture>;
  static auto construct(const Device &, const FS::Path &) -> Scope<Texture>;
  static auto construct_from_buffer(const Device &, const TextureProperties &,
                                    DataBuffer &&) -> Scope<Texture>;

private:
  Texture(const Device &, const TextureProperties &);
  Texture(const Device &, usize, const Extent<u32> &);
  Texture(const Device &, const TextureProperties &, DataBuffer &&);

  const Device *device{nullptr};
  TextureProperties properties;

  DataBuffer data_buffer;
  usize cached_size{0};
  bool storage{false};

  Scope<Image> image{nullptr};
};

} // namespace Core
