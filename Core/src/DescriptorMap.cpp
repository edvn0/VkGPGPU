#include "DescriptorMap.hpp"

#include "pch/vkgpgpu_pch.hpp"

#include "Config.hpp"
#include "Device.hpp"
#include "Formatters.hpp"
#include "Types.hpp"
#include "Verify.hpp"

#include <array>
#include <fmt/ranges.h>
#include <ranges>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

static constexpr auto range = [](std::integral auto begin,
                                 std::integral auto end) {
  return std::views::iota(begin, end);
};

static constexpr auto for_frame = []() {
  return range(0, Core::Config::frame_count);
};

namespace Core {

DescriptorMap::DescriptorMap(const Device &dev) : device(dev) {

  static constexpr auto sets_per_frame = 2;
  for (const auto i : for_frame()) {
    descriptors().try_emplace(i, sets_per_frame);
  }
  descriptor_set_layout.resize(sets_per_frame);

  std::array<VkDescriptorPoolSize, 3> pool_sizes{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_sizes[0].descriptorCount = Config::frame_count * 10;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[1].descriptorCount = Config::frame_count * 10;
  pool_sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  pool_sizes[2].descriptorCount = Config::frame_count * 10;

  // Create the descriptor pool
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.data();
  pool_info.maxSets = Config::frame_count * (3 + 1 + 2);

  verify(vkCreateDescriptorPool(device.get_device(), &pool_info, nullptr,
                                &descriptor_pool),
         "vkCreateDescriptorPool", "Failed to create descriptor pool!");

  static constexpr auto create_first_set_layout = [](auto &device,
                                                     auto &current_layout) {
    VkDescriptorSetLayoutBinding binding_0{};
    binding_0.binding = 0;
    binding_0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding_0.descriptorCount = 1;
    binding_0.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding binding_1{};
    binding_1.binding = 1;
    binding_1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding_1.descriptorCount = 1;
    binding_1.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding binding_2{};
    binding_2.binding = 2;
    binding_2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding_2.descriptorCount = 1;
    binding_2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding uniform{};
    uniform.binding = 3;
    uniform.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniform.descriptorCount = 1;
    uniform.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    const std::array bindings{binding_0, binding_1, binding_2, uniform};

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<u32>(bindings.size());
    layout_info.pBindings = bindings.data();

    verify(vkCreateDescriptorSetLayout(device.get_device(), &layout_info,
                                       nullptr, &current_layout),
           "vkCreateDescriptorSetLayout",
           "Failed to create descriptor set layout!");
  };

  static constexpr auto create_second_set_layout = [](auto &device,
                                                      auto &current_layout) {
    VkDescriptorSetLayoutBinding binding_0{};
    binding_0.binding = 0;
    binding_0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding_0.descriptorCount = 1;
    binding_0.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding binding_1{};
    binding_1.binding = 1;
    binding_1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding_1.descriptorCount = 1;
    binding_1.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    const std::array bindings{binding_0, binding_1};

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<u32>(bindings.size());
    layout_info.pBindings = bindings.data();

    verify(vkCreateDescriptorSetLayout(device.get_device(), &layout_info,
                                       nullptr, &current_layout),
           "vkCreateDescriptorSetLayout",
           "Failed to create descriptor set layout!");
  };

  create_first_set_layout(device, descriptor_set_layout.at(0));
  create_second_set_layout(device, descriptor_set_layout.at(1));

  std::vector<VkDescriptorSetLayout> layouts;
  for (const auto i : for_frame()) {
    layouts.push_back(descriptor_set_layout.at(0));
    layouts.push_back(descriptor_set_layout.at(1));
  }

  VkDescriptorSetAllocateInfo allocation_info{};
  allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation_info.descriptorPool = descriptor_pool;
  allocation_info.pSetLayouts = layouts.data();

  for (const auto i : for_frame()) {
    auto &frame_descriptors = descriptors().at(i);
    allocation_info.descriptorSetCount =
        static_cast<u32>(frame_descriptors.size());

    verify(vkAllocateDescriptorSets(device.get_device(), &allocation_info,
                                    frame_descriptors.data()),
           "vkAllocateDescriptorSets", "Failed to allocate descriptor sets!");
  }
}

DescriptorMap::~DescriptorMap() {
  auto *vk_device = device.get_device();
  vkDestroyDescriptorPool(vk_device, descriptor_pool, nullptr);

  for (auto &layout : descriptor_set_layout) {
    vkDestroyDescriptorSetLayout(vk_device, layout, nullptr);
  }
}

auto DescriptorMap::bind(const CommandBuffer &buffer, u32 current_frame,
                         VkPipelineLayout layout) const -> void {
  const auto &current_descriptor = descriptors().at(current_frame);

  vkCmdBindDescriptorSets(buffer.get_command_buffer(),
                          VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0,
                          static_cast<u32>(current_descriptor.size()),
                          current_descriptor.data(), 0, nullptr);
}

auto DescriptorMap::get_descriptor_pool() -> VkDescriptorPool {
  return descriptor_pool;
}

auto DescriptorMap::add_for_frames(u32 binding, const Buffer &buffer) -> void {
  static constexpr auto add_for_frame = [](auto &dev, auto chosen_set,
                                           auto chosen_binding, auto &sets,
                                           auto &chosen_buffer) {
    // Let's begin by doing this for one frame
    const auto &buffer_info = chosen_buffer.get_descriptor_info();

    VkWriteDescriptorSet descriptor_write{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = sets.at(chosen_set);
    descriptor_write.dstBinding = chosen_binding;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorType = chosen_buffer.get_vulkan_type();
    descriptor_write.descriptorCount = 1;
    descriptor_write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(dev.get_device(), 1, &descriptor_write, 0, nullptr);
    info("Updated descriptor {} at set {} and binding {}!",
         fmt::ptr(descriptor_write.dstSet), chosen_set, chosen_binding);
  };

  for (const auto i : for_frame()) {
    add_for_frame(device, 0, binding, descriptors().at(i), buffer);
  }
}

auto DescriptorMap::add_for_frames(u32 binding, const Image &image) -> void {
  static constexpr auto add_for_frame = [](auto &dev, auto chosen_set,
                                           auto chosen_binding, auto &sets,
                                           auto &chosen_image) {
    // Let's begin by doing this for one frame
    const auto &image_info = chosen_image.get_descriptor_info();

    VkWriteDescriptorSet descriptor_write{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = sets.at(chosen_set);
    descriptor_write.dstBinding = chosen_binding;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorType = chosen_image.get_vulkan_type();
    descriptor_write.descriptorCount = 1;
    descriptor_write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(dev.get_device(), 1, &descriptor_write, 0, nullptr);
    info("Updated descriptor {} at set {} and binding {}!",
         fmt::ptr(descriptor_write.dstSet), chosen_set, chosen_binding);
  };

  for (const auto i : for_frame()) {
    add_for_frame(device, 1, binding, descriptors().at(i), image);
  }
}

auto DescriptorMap::add_for_frames(u32 binding, const Texture &texture)
    -> void {
  ensure(texture.valid(), "Texture was invalid");
  add_for_frames(binding, texture.get_image());
}

} // namespace Core
