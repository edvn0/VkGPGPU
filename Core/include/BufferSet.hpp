#pragma once

#include "Buffer.hpp"
#include "Config.hpp"
#include "Device.hpp"
#include "Types.hpp"
#include "Verify.hpp"

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

  auto set_frame_count(u32 frames) -> void {
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

  [[nodiscard]] auto get_bindings(DescriptorSet set = 0) const
      -> std::vector<VkDescriptorSetLayoutBinding> {
    const auto &bindings = frame_set_binding_buffers.at(0).at(set);
    std::vector<VkDescriptorSetLayoutBinding> result;
    result.reserve(bindings.size());
    for (const auto &[binding, buffer] : bindings) {
      result.push_back(VkDescriptorSetLayoutBinding{
          .binding = binding,
          .descriptorType = buffer->get_vulkan_type(),
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_ALL,
          .pImmutableSamplers = nullptr,
      });
    }
    return result;
  }

  [[nodiscard]] auto get_write_descriptors(u32 current_frame,
                                           DescriptorSet set = 0) const
      -> std::vector<VkWriteDescriptorSet> {
    static std::unordered_map<u32, std::vector<VkWriteDescriptorSet>>
        write_descriptor_cache;
    if (write_descriptor_cache.contains(current_frame)) {
      return write_descriptor_cache.at(current_frame);
    }

    const auto &bindings = frame_set_binding_buffers.at(current_frame).at(set);
    std::vector<VkWriteDescriptorSet> result;
    result.reserve(bindings.size());
    for (const auto &[binding, buffer] : bindings) {
      result.push_back(VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = nullptr,
          .dstSet = nullptr,
          .dstBinding = binding,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = buffer->get_vulkan_type(),
          .pImageInfo = nullptr,
          .pBufferInfo = &buffer->get_descriptor_info(),
          .pTexelBufferView = nullptr,
      });
    }

    write_descriptor_cache[current_frame + set * Config::frame_count] = result;

    return result;
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
