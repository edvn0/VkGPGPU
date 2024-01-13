#include "Texture.hpp"

#include "Formatters.hpp"

#include <stb_image.h>
#include <stb_image_write.h>

namespace Core {

namespace {} // namespace

Texture::~Texture() = default;

Texture::Texture(const Device &dev, const FS::Path &path)
    : Texture(dev, load_databuffer_from_file(path)) {}

Texture::Texture(const Device &dev, DataBuffer &&buffer)
    : device(&dev), data_buffer(std::move(buffer)) {
  info("Creating texture from buffer of size {}", data_buffer.size());
  image =
      make_scope<Image>(*device,
                        ImageProperties{
                            .extent = {20, 20},
                            .format = ImageFormat::R8G8B8A8Unorm,
                            .tiling = ImageTiling::Linear,
                            .usage = ImageUsage::Sampled | ImageUsage::Storage,
                            .layout = ImageLayout::General,
                        },
                        data_buffer);
}

auto Texture::valid() const noexcept -> bool {
  return data_buffer.valid() && image != nullptr;
}

auto Texture::get_image_info() const noexcept -> const VkDescriptorImageInfo & {
  return image->get_descriptor_info();
}

auto Texture::write_to_file(const FS::Path &path) -> bool {
  // ensure that parent path exists
  if (const auto parent_path = FS::resolve(path).parent_path();
      !FS::exists(parent_path)) {
    return false;
  }

  std::vector<Core::u8> buffer;
  buffer.resize(data_buffer.size());
  data_buffer.read(buffer, buffer.size());

  const auto &extent = image->get_extent();
  const auto [width, height] = extent.as<std::int32_t>();

  stbi_write_png(path.string().c_str(), width, height, 4, buffer.data(), 0);

  return true;
}

} // namespace Core
