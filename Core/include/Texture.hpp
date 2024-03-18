#pragma once

#include "DataBuffer.hpp"
#include "Device.hpp"
#include "Filesystem.hpp"
#include "Image.hpp"
#include "ImageProperties.hpp"

#include <variant>
#include <vulkan/vulkan.h>

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

enum class ResizeMethod : u8 {
  ByScalingFactor, // Resize based on a scaling factor (e.g., 0.5 for half size)
  ByAbsoluteSize,  // Resize to specific dimensions
  Default,         // Does nothing, uses the original image size
};

// Scaling factor data, specifying how much to scale the original image
struct ScalingFactorData {
  float scale_factor;
};

class ResizeStrategy {
public:
  // Strategy can be either a method enum or specific data for resizing
  std::variant<ResizeMethod, Extent<u32>, ScalingFactorData> strategy;

  // Default constructor uses the Default resizing method
  ResizeStrategy() : strategy(ResizeMethod::Default) {}

  // Constructor for setting the strategy directly with ResizeMethod
  explicit ResizeStrategy(ResizeMethod method) : strategy(method) {}

  // Constructors for specific resizing strategies
  explicit ResizeStrategy(u32 width, u32 height)
      : strategy(Extent<u32>{width, height}) {}

  explicit ResizeStrategy(const Extent<u32> &extent) : strategy(extent) {}

  explicit ResizeStrategy(float scale_factor)
      : strategy(ScalingFactorData{scale_factor}) {}

  // Additional constructor to handle more complex logic
  ResizeStrategy(ResizeMethod method, u32 width, u32 height) {
    if (method == ResizeMethod::ByAbsoluteSize) {
      strategy = Extent<u32>{width, height};
    } else if (method == ResizeMethod::ByScalingFactor) {
      // Should not really call this constructor with a scaling factor
      strategy = ScalingFactorData{1.0F};
    } else {
      strategy = method;
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
  ResizeStrategy resize{ResizeStrategy(ResizeMethod::Default)};
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

  auto transition_image(ImageLayout) -> void;

  [[nodiscard]] auto hash() const -> usize;
  [[nodiscard]] auto get_file_path() const
      -> std::optional<std::filesystem::path> {

    if (properties.path.empty()) {
      return std::nullopt;
    }

    return properties.path;
  }

  auto get_mip_size(u32 mip) -> std::pair<u32, u32> {
    return image->get_mip_size(mip);
  }

  static auto empty_with_size(const Device &, usize, const Extent<u32> &)
      -> Scope<Texture>;
  static auto construct(const Device &, const TextureProperties &)
      -> Scope<Texture>;
  static auto construct_from_command_buffer(const Device &,
                                            const TextureProperties &,
                                            CommandBuffer &) -> Scope<Texture>;
  static auto construct_from_command_buffer(const Device &,
                                            const TextureProperties &,
                                            DataBuffer &&, CommandBuffer &)
      -> Scope<Texture>;
  static auto construct_storage(const Device &, const TextureProperties &)
      -> Scope<Texture>;
  static auto construct_shader(const Device &, const TextureProperties &)
      -> Scope<Texture>;
  static auto construct(const Device &, const FS::Path &) -> Scope<Texture>;
  static auto construct_from_buffer(const Device &, const TextureProperties &,
                                    DataBuffer &&) -> Scope<Texture>;

private:
  Texture(const Device &, const TextureProperties &, CommandBuffer * = nullptr);
  Texture(const Device &, const TextureProperties &, DataBuffer &&,
          CommandBuffer *);
  Texture(const Device &, const TextureProperties &, CommandBuffer &);
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
