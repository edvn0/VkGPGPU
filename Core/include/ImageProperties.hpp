#pragma once

#include <type_traits>
namespace Core {

template <std::integral T> struct Extent {
  T width{0};
  T height{0};

  // Cast to another type Other, not the same as T
  template <std::integral Other>
    requires(!std::is_same_v<Other, T>)
  auto as() const {
    return Extent<Other>{
        .width = static_cast<Other>(width),
        .height = static_cast<Other>(height),
    };
  }
};

enum class ImageTiling : std::uint8_t {
  Optimal,
  Linear,
};

static constexpr auto bit(std::size_t i) {
  return static_cast<std::uint32_t>(1) << i;
}

enum class ImageUsage : std::uint8_t {
  TransferSrc = bit(0),
  TransferDst = bit(1),
  Sampled = bit(2),
  Storage = bit(3),
  ColorAttachment = bit(4),
  DepthStencilAttachment = bit(5),
  TransientAttachment = bit(6),
  InputAttachment = bit(7),
};
constexpr auto operator|(ImageUsage lhs, ImageUsage rhs) {
  return static_cast<ImageUsage>(static_cast<std::byte>(lhs) |
                                 static_cast<std::byte>(rhs));
}
constexpr auto operator&(ImageUsage lhs, ImageUsage rhs) {
  return static_cast<ImageUsage>(static_cast<std::byte>(lhs) &
                                 static_cast<std::byte>(rhs));
}

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

enum class ImageFormat : std::uint8_t {
  Undefined,
  R4G4UnormPack8,
  R4G4B4A4UnormPack16,
  B4G4R4A4UnormPack16,
  R5G6B5UnormPack16,
  B5G6R5UnormPack16,
  R5G5B5A1UnormPack16,
  B5G5R5A1UnormPack16,
  A1R5G5B5UnormPack16,
  R8Unorm,
  R8Snorm,
  R8Uscaled,
  R8Sscaled,
  R8Uint,
  R8Sint,
  R8Srgb,
  R8G8Unorm,
  R8G8Snorm,
  R8G8Uscaled,
  R8G8Sscaled,
  R8G8Uint,
  R8G8Sint,
  R8G8Srgb,
  R8G8B8Unorm,
  R8G8B8Snorm,
  R8G8B8Uscaled,
  R8G8B8Sscaled,
  R8G8B8Uint,
  R8G8B8Sint,
  R8G8B8Srgb,
  B8G8R8Unorm,
  B8G8R8Snorm,
  B8G8R8Uscaled,
  B8G8R8Sscaled,
  B8G8R8Uint,
  B8G8R8Sint,
  B8G8R8Srgb,
  R8G8B8A8Unorm,
  R8G8B8A8Snorm,
  R8G8B8A8Uscaled,
  R8G8B8A8Sscaled,
  R8G8B8A8Uint,
  R8G8B8A8Sint,
  R8G8B8A8Srgb,
  B8G8R8A8Unorm,
  B8G8R8A8Snorm,
  B8G8R8A8Uscaled,
  B8G8R8A8Sscaled,
  B8G8R8A8Uint,
  B8G8R8A8Sint
};

enum class SamplerFilter : std::uint8_t {
  Nearest,
  Linear,
};

enum class SamplerAddressMode : std::uint8_t {
  Repeat,
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

template <class T>
  requires std::is_enum_v<T>
struct ToFromStringView {
  static auto to_string(T t) -> std::string_view = delete;
  static auto from_string(std::string_view sv) -> T = delete;
};

} // namespace Core
