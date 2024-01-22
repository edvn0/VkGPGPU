#include "pch/vkgpgpu_pch.hpp"

#include "Texture.hpp"

#include "DataBuffer.hpp"
#include "Formatters.hpp"
#include "Types.hpp"

#include <stb_image.h>
#include <stb_image_write.h>

namespace Core {

auto Texture::empty_with_size(const Device &device, usize size,
                              const Extent<u32> &extent) -> Scope<Texture> {
  auto texture = make_scope<Texture>(device, size, extent);
  return texture;
}

auto Texture::construct(const Device &device,
                        const TextureProperties &properties) -> Scope<Texture> {
  auto texture = make_scope<Texture>(device, properties);
  return texture;
}

auto Texture::construct(const Device &device, const FS::Path &path)
    -> Scope<Texture> {
  TextureProperties properties;
  properties.path = path;
  auto texture = Texture::construct(device, properties);
  return texture;
}

Texture::~Texture() {
  info("Destroyed Texture '{}', size: {}", properties.identifier,
       data_buffer.size());
}

Texture::Texture(const Device &dev, usize size, const Extent<u32> &extent)
    : device(&dev), data_buffer(size) {
  properties.extent = extent;
  properties.identifier = fmt::format("Empty-Size{}", size);
  data_buffer.fill_zero();
  image = make_scope<Image>(
      *device,
      ImageProperties{
          .extent = properties.extent,
          .format = ImageFormat::R8G8B8A8Unorm,
          .tiling = ImageTiling::Linear,
          .usage = ImageUsage::Sampled | ImageUsage::Storage,
          .layout = ImageLayout::General,
          .min_filter = SamplerFilter::Linear,
          .max_filter = SamplerFilter::Linear,
          .address_mode = SamplerAddressMode::Repeat,
          .border_color = SamplerBorderColor::FloatOpaqueBlack,
      },
      data_buffer);
  info("Created texture '{}', {}", properties.identifier, properties.extent);
}

Texture::Texture(const Device &dev, const TextureProperties &props)
    : device(&dev), properties(props),
      data_buffer(
          load_databuffer_from_file(properties.path, properties.extent)) {
  if (!FS::exists(properties.path)) {
    throw NotFoundException(fmt::format("Texture file '{}' does not exist!",
                                        properties.path.string()));
  }

  properties.identifier = properties.path.filename().string();

  image = make_scope<Image>(
      *device,
      ImageProperties{
          .extent = properties.extent,
          .format = ImageFormat::R8G8B8A8Unorm,
          .tiling = ImageTiling::Linear,
          .usage = ImageUsage::Sampled | ImageUsage::Storage,
          .layout = ImageLayout::General,
          .min_filter = SamplerFilter::Linear,
          .max_filter = SamplerFilter::Linear,
          .address_mode = SamplerAddressMode::Repeat,
          .border_color = SamplerBorderColor::FloatOpaqueBlack,
      },
      data_buffer);
  info("Created texture '{}', {}", properties.identifier, properties.extent);
}

auto Texture::hash() const -> usize {
  static constexpr std::hash<std::string> string_hash;
  auto name_hash = string_hash(properties.identifier);
  return name_hash ^ data_buffer.hash();
}

auto Texture::valid() const noexcept -> bool {
  return data_buffer.valid() && image != nullptr;
}

auto Texture::get_image_info() const noexcept -> const VkDescriptorImageInfo & {
  return image->get_descriptor_info();
}

auto Texture::get_image() const noexcept -> const Image & { return *image; }

auto Texture::write_to_file(const FS::Path &path) -> bool {
  // ensure that parent path exists
  if (const auto parent_path = FS::resolve(path).parent_path();
      !FS::exists(parent_path)) {
    return false;
  }

  std::vector<Core::u8> buffer;
  buffer.resize(data_buffer.size());
  data_buffer.read(buffer, buffer.size());

  const auto &ext = image->get_extent();
  const auto [width, height] = ext.as<std::int32_t>();

  stbi_write_png(path.string().c_str(), width, height, 4, buffer.data(), 0);

  return true;
}

} // namespace Core
