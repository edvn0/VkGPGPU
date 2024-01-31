namespace Core::Destructors {

// Specialization for VkImageView
template <>
inline auto destroy<VkImageView>(const Device &device,
                                 const VkImageView &type) {
  vkDestroyImageView(device.get_device(), type, nullptr);
}

template <>
inline auto destroy<VkDescriptorPool>(const Device &device,
                                      const VkDescriptorPool &type) {
  vkDestroyDescriptorPool(device.get_device(), type, nullptr);
}

template <>
inline auto destroy<VkDescriptorSetLayout>(const Device &device,
                                           const VkDescriptorSetLayout &type) {
  vkDestroyDescriptorSetLayout(device.get_device(), type, nullptr);
}

} // namespace Core::Destructors
