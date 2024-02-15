#pragma once

#include "Concepts.hpp"
#include "Types.hpp"

#include <fmt/core.h>
#include <type_traits>
#include <vulkan/vulkan.h>

namespace Core {

#define MAKE_BITFIELD(TYPE, BaseType)                                          \
  constexpr auto operator|(TYPE lhs, TYPE rhs) {                               \
    return static_cast<TYPE>(static_cast<BaseType>(lhs) |                      \
                             static_cast<BaseType>(rhs));                      \
  }                                                                            \
  constexpr auto operator|=(TYPE &lhs, TYPE rhs) {                             \
    lhs = static_cast<TYPE>(static_cast<BaseType>(lhs) |                       \
                            static_cast<BaseType>(rhs));                       \
    return lhs;                                                                \
  }                                                                            \
  constexpr auto operator&(TYPE lhs, TYPE rhs) {                               \
    return static_cast<TYPE>(static_cast<BaseType>(lhs) &                      \
                             static_cast<BaseType>(rhs));                      \
  }                                                                            \
  constexpr auto operator&=(TYPE &lhs, TYPE rhs) {                             \
    lhs = static_cast<TYPE>(static_cast<BaseType>(lhs) &                       \
                            static_cast<BaseType>(rhs));                       \
    return lhs;                                                                \
  }

template <IsNumber T> struct Extent {
  T width{0};
  T height{0};

  // Aspect ratio
  [[nodiscard]] constexpr auto aspect_ratio() const noexcept -> floating {
    return static_cast<floating>(width) / static_cast<floating>(height);
  }

  [[nodiscard]] auto valid() const noexcept -> bool {
    return width > 0 && height > 0;
  }

  // Cast to another type Other, not the same as T
  template <typename Other>
    requires(!std::is_same_v<Other, T> &&
             (std::is_integral_v<Other> || std::is_floating_point_v<Other>))
  auto as() const {
    return Extent<Other>{
        .width = static_cast<Other>(width),
        .height = static_cast<Other>(height),
    };
  }

  template <IsNumber U>
    requires(!std::is_same_v<U, T>)
  auto operator==(const Extent<U> &rhs) const -> bool {
    return width == rhs.width && height == rhs.height;
  }
  template <IsNumber U>
    requires(!std::is_same_v<U, T>)
  auto operator!=(const Extent<U> &rhs) const -> bool {
    return !(*this == rhs);
  }
  auto operator==(const Extent &rhs) const -> bool = default;
  auto operator!=(const Extent &rhs) const -> bool = default;
};

enum class ImageTiling : std::uint8_t {
  Optimal,
  Linear,
};

static constexpr auto bit(std::size_t i) { return static_cast<u32>(1) << i; }

enum class ImageUsage : std::uint8_t {
  TransferSrc = bit(0),
  TransferDst = bit(1),
  Sampled = bit(2),
  Storage = bit(3),
  ColourAttachment = bit(4),
  DepthStencilAttachment = bit(5),
  TransientAttachment = bit(6),
  InputAttachment = bit(7),
};
MAKE_BITFIELD(ImageUsage, std::uint8_t)

enum class ImageLayout : std::uint16_t {
  Undefined = bit(0),
  General = bit(1),
  ColorAttachmentOptimal = bit(2),
  DepthAttachmentOptimal = bit(3),
  DepthStencilAttachmentOptimal = bit(4),
  DepthStencilReadOnlyOptimal = bit(5),
  ShaderReadOnlyOptimal = bit(6),
  TransferSrcOptimal = bit(7),
  TransferDstOptimal = bit(8),
  Preinitialized = bit(9),
  DepthReadOnlyStencilAttachmentOptimal = bit(10),
  DepthAttachmentStencilReadOnlyOptimal = bit(11),
  PresentSrcKHR = bit(12),
  SharedPresentKHR = bit(13),
  ShadingRateOptimalNV = bit(14),
  FragmentDensityMapOptimalEXT = bit(15),
};
MAKE_BITFIELD(ImageLayout, std::uint16_t)

enum class ImageFormat : std::uint8_t {
  Undefined,
  SRGB_RGBA8,
  SRGB_RGBA32,
  UNORM_RGBA8,
  DEPTH32F,
  DEPTH24STENCIL8,
  DEPTH16
};
auto to_vulkan_format(ImageFormat format) -> VkFormat;

enum class SamplerFilter : std::uint8_t {
  Nearest = 0,
  Linear,
};

enum class SamplerAddressMode : std::uint8_t {
  Repeat = 0,
  MirroredRepeat,
  ClampToEdge,
  ClampToBorder,
  MirrorClampToEdge,
};

enum class SamplerBorderColor : std::uint8_t {
  FloatTransparentBlack,
  IntTransparentBlack,
  FloatOpaqueBlack,
  IntOpaqueBlack,
  FloatOpaqueWhite,
  IntOpaqueWhite,
};

enum class CompareOperation : std::uint8_t {
  Never,
  Less,
  Equal,
  LessOrEqual,
  Greater,
  NotEqual,
  GreaterOrEqual,
  Always,
};

template <class T>
  requires std::is_enum_v<T>
struct ToFromStringView {
  static auto to_string(T t) -> std::string_view = delete;
  static auto from_string(std::string_view sv) -> T = delete;
};

} // namespace Core

template <>
struct fmt::formatter<Core::Extent<Core::u32>> : fmt::formatter<const char *> {
  auto format(const Core::Extent<Core::u32> &extent, format_context &ctx) const
      -> decltype(ctx.out());
};
