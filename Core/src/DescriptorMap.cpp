#include "Types.hpp"
#include "pch/vkgpgpu_pch.hpp"

#include "Config.hpp"
#include "DescriptorMap.hpp"
#include "Device.hpp"
#include "Formatters.hpp"
#include "Verify.hpp"

#include <array>
#include <fmt/ranges.h>
#include <ranges>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Core {

	DescriptorMap::DescriptorMap(const Device& dev)
		: device(dev) {

		for (auto i = 0; i < Config::frame_count; ++i) {
			descriptors().try_emplace(i, 1);
		}

		// Create pool sizes for 10 storage buffers and 10 uniform buffers
		std::array<VkDescriptorPoolSize, 2> pool_sizes{};
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		pool_sizes[0].descriptorCount = Config::frame_count * 2;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_sizes[1].descriptorCount = Config::frame_count;

		// Create the descriptor pool
		VkDescriptorPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
		pool_info.pPoolSizes = pool_sizes.data();
		pool_info.maxSets = 100;

		verify(vkCreateDescriptorPool(device.get_device(), &pool_info, nullptr,
			&descriptor_pool),
			"vkCreateDescriptorPool", "Failed to create descriptor pool!");

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

		VkDescriptorSetLayoutBinding uniform{};
		uniform.binding = 2;
		uniform.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniform.descriptorCount = 1;
		uniform.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		const std::array bindings{ binding_0, binding_1, uniform };

		VkDescriptorSetLayoutCreateInfo layout_info{};
		layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = static_cast<u32>(bindings.size());
		layout_info.pBindings = bindings.data();

		verify(vkCreateDescriptorSetLayout(device.get_device(), &layout_info, nullptr,
			&descriptor_set_layout),
			"vkCreateDescriptorSetLayout",
			"Failed to create descriptor set layout!");

		const std::vector layouts(Config::frame_count, descriptor_set_layout);
		VkDescriptorSetAllocateInfo allocation_info{};
		allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocation_info.descriptorPool = descriptor_pool;
		allocation_info.descriptorSetCount = 1;
		allocation_info.pSetLayouts = layouts.data();

		for (auto i = 0; i < Config::frame_count; ++i) {
			auto& frame_descriptors = descriptors().at(i);

			verify(vkAllocateDescriptorSets(device.get_device(), &allocation_info,
				frame_descriptors.data()),
				"vkAllocateDescriptorSets", "Failed to allocate descriptor sets!");
		}
	}

	DescriptorMap::~DescriptorMap() {
		auto* vk_device = device.get_device();
		vkDestroyDescriptorPool(vk_device, descriptor_pool, nullptr);
		vkDestroyDescriptorSetLayout(vk_device, descriptor_set_layout,
			nullptr);
	}

	auto DescriptorMap::bind(const CommandBuffer& buffer, u32 current_frame,
		VkPipelineLayout layout) const -> void {
		const auto& current_descriptor = descriptors().at(current_frame);

		vkCmdBindDescriptorSets(buffer.get_command_buffer(),
			VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0,
			static_cast<u32>(current_descriptor.size()),
			current_descriptor.data(), 0, nullptr);
	}

	auto DescriptorMap::get_descriptor_pool() -> VkDescriptorPool {
		return descriptor_pool;
	}

	auto DescriptorMap::add_for_frames(u32 set, u32 binding, const Buffer& buffer)
		-> void {
		static constexpr auto add_for_frame = [](auto& dev, auto chosen_set,
			auto chosen_binding,
			auto& sets,
			auto& chosen_buffer) {
				// Let's begin by doing this for one frame
				const auto& buffer_info = chosen_buffer.get_descriptor_info();

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

		for (const auto i : std::views::iota(0, Config::frame_count)) {
			add_for_frame(device, set, binding, descriptors().at(i), buffer);
		}
	}

} // namespace Core