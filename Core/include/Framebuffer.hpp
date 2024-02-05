#pragma once

#include "Image.hpp"
#include "Math.hpp"
#include "ResizeDependent.hpp"
#include "Types.hpp"

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Core {

class Framebuffer;

enum class FramebufferBlendMode {
  None = 0,
  OneZero,
  OneMinusSourceAlpha,
  Additive,
  ZeroSourceColor
};

struct FramebufferTextureSpecification {
  ImageFormat format{ImageFormat::SRGB_RGBA8};
  bool blend{true};
  FramebufferBlendMode blend_mode{FramebufferBlendMode::OneMinusSourceAlpha};
};

struct FramebufferAttachmentSpecification {
  std::vector<FramebufferTextureSpecification> attachments;
  // std::initializer_list
  explicit FramebufferAttachmentSpecification(
      std::initializer_list<FramebufferTextureSpecification> attachments)
      : attachments(attachments) {}
};

struct FramebufferProperties {
  u32 width{0};
  u32 height{0};
  const bool resizeable{true};
  const floating scale{1.0f};
  const Math::Vec4 clear_colour{0.0f, 0.0f, 0.0f, 1.0f};
  const floating depth_clear_value{0.0f}; // We use inverted z-buffer
  const bool clear_colour_on_load{true};
  const bool clear_depth_on_load{true};

  const bool blend{true};
  const bool invert_viewport{true};
  FramebufferBlendMode blend_mode = FramebufferBlendMode::None;

  FramebufferAttachmentSpecification attachments;

  const bool transfer{false};

  Ref<Image> existing_image;
  std::vector<u32> existing_image_layers;

  std::unordered_map<u32, Ref<Image>> existing_images;

  Ref<Framebuffer> existing_framebuffer;

  std::string debug_name;
};

class Framebuffer {
public:
  ~Framebuffer();

  auto on_resize(u32, u32, bool should_clean = false) -> void;
  auto on_resize(const Extent<u32> &) -> void;

  [[nodiscard]] auto get_width() const -> u32 { return width; }
  [[nodiscard]] auto get_height() const -> u32 { return height; }

  [[nodiscard]] auto get_image(u32 attachment_index = 0) const
      -> const Ref<Image> & {
    return attachment_images.at(attachment_index);
  }

  [[nodiscard]] auto get_depth_image() const -> const Ref<Image> & {
    return depth_attachment_image;
  }
  [[nodiscard]] auto get_colour_attachment_count() const -> usize {
    return attachment_images.size();
  }
  [[nodiscard]] auto has_depth_attachment() const -> bool {
    return static_cast<bool>(depth_attachment_image);
  }
  [[nodiscard]] auto get_render_pass() const -> VkRenderPass {
    return render_pass;
  }
  [[nodiscard]] auto get_framebuffer() const -> VkFramebuffer {
    return framebuffer;
  }
  [[nodiscard]] auto get_clear_values() const
      -> const std::vector<VkClearValue> & {
    return clear_values;
  }
  [[nodiscard]] auto get_properties() const -> const auto & {
    return properties;
  }
  [[nodiscard]] auto get_extent() const -> Extent<u32> {
    return {width, height};
  }

  auto add_resize_dependent(IResizeDependent<Framebuffer> *dependent) -> void {
    if (resize_dependents.contains(dependent))
      return;
    resize_dependents.insert(dependent);
  }

  auto invalidate() -> void;

  static auto construct(const Device &, const FramebufferProperties &)
      -> Scope<Framebuffer>;

private:
  Framebuffer(const Device &, const FramebufferProperties &);
  const Device *device;
  FramebufferProperties properties;
  u32 width{0};
  u32 height{0};

  std::vector<Ref<Image>> attachment_images;
  Ref<Image> depth_attachment_image;

  std::vector<VkClearValue> clear_values;

  VkRenderPass render_pass = nullptr;
  VkFramebuffer framebuffer = nullptr;

  auto clean() -> void;
  auto create_framebuffer() -> void;

  std::unordered_set<IResizeDependent<Framebuffer> *> resize_dependents{};
};

} // namespace Core
