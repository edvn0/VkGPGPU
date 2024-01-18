#pragma once

#include "Buffer.hpp"
#include "Config.hpp"
#include "Device.hpp"
#include "Types.hpp"

#include <ranges>

namespace Core {

using DescriptorSet = u32;
using FrameIndex = u32;
using DescriptorBinding = u32;

struct SetBinding {
  DescriptorBinding binding;
  DescriptorSet set{0};

  constexpr explicit SetBinding(DescriptorBinding bind) : binding(bind){};
  constexpr SetBinding(DescriptorSet desc_set, DescriptorBinding bind)
      : binding(bind), set(desc_set) {}
};

template <Buffer::Type Type> class BufferSet {
public:
  explicit BufferSet(const Device &dev, u32 frames = Config::frame_count)
      : device(&dev), frame_count(frames), frame_set_binding_buffers(frames) {
    info("Created buffer set of type '{}' with {} frame count", Type,
         frame_count);
  }

  ~BufferSet() = default;

  auto set_frame_count(std::uint32_t frames) -> void {
    ensure(frame_set_binding_buffers.empty(),
           "BufferSet must be initialized before setting frame count");
    ensure(frames > 0,
           "BufferSet must be initialized with a frame count greater than 0");
    frame_count = frames;
  }

  auto create(std::integral auto size, SetBinding layout) -> void {
    for (auto frame = static_cast<FrameIndex>(0); frame < frame_count;
         ++frame) {
      auto buffer = Buffer::construct(*device, static_cast<u64>(size), Type,
                                      layout.binding);
      set(std::move(buffer), frame, layout.set);
    }
  }

  auto get(DescriptorBinding binding, FrameIndex frame_index,
           DescriptorSet set = 0) -> auto & {
    ensure(frame_set_binding_buffers.contains(frame_index),
           "BufferSet does not contain frame index");
    ensure(frame_set_binding_buffers.at(frame_index).contains(set),
           "BufferSet does not contain descriptor set");
    ensure(frame_set_binding_buffers.at(frame_index).at(set).contains(binding),
           "BufferSet does not contain descriptor binding");
    return frame_set_binding_buffers.at(frame_index).at(set).at(binding);
  }

  auto set(Scope<Buffer> &&buffer, FrameIndex frame_index,
           DescriptorSet set = DescriptorSet{0}) -> void {
    ensure(frame_index < frame_count, "BufferSet frame index out of range");
    frame_set_binding_buffers[frame_index][set].try_emplace(
        buffer->get_binding(), std::move(buffer));
  }

  static auto construct(const Device &device) -> Scope<BufferSet> {
    return make_scope<BufferSet>(device);
  }

private:
  const Device *device;

  u32 frame_count{0};
  using BindingBuffers = std::unordered_map<DescriptorBinding, Scope<Buffer>>;
  using SetBindingBuffers = std::unordered_map<DescriptorSet, BindingBuffers>;

  std::unordered_map<FrameIndex, SetBindingBuffers> frame_set_binding_buffers{};
};

} // namespace Core
