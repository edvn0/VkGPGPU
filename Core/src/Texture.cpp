#include "pch/vkgpgpu_pch.hpp"

#include "Texture.hpp"

#include "DataBuffer.hpp"
#include "Formatters.hpp"
#include "Types.hpp"
#include "Verify.hpp"

#include <stb_image.h>
#include <stb_image_write.h>

namespace Core {

auto Texture::empty_with_size(const Device &device, usize size,
                              const Extent<u32> &extent) -> Scope<Texture> {
  return Scope<Texture>(new Texture(device, size, extent));
}

auto Texture::construct(const Device &device,
                        const TextureProperties &properties) -> Scope<Texture> {
  return Scope<Texture>(new Texture(device, properties));
}

auto Texture::construct_shader(const Device &device,
                               const TextureProperties &properties)
    -> Scope<Texture> {
  ensure((properties.usage & ImageUsage::Sampled) != ImageUsage{0},
         "Texture must be sampled");
  ensure(properties.layout == ImageLayout::ShaderReadOnlyOptimal,
         "Texture must be ShaderReadOnlyOptimal");

  return Scope<Texture>(new Texture(device, properties));
}

auto Texture::construct_storage(const Device &device,
                                const TextureProperties &properties)
    -> Scope<Texture> {
  ensure((properties.usage & ImageUsage::Storage) != ImageUsage{0},
         "Texture must be storage");
  ensure(properties.layout == ImageLayout::General, "Texture must be General");
  return Scope<Texture>(new Texture(device, properties));
}

auto Texture::construct(const Device &device, const FS::Path &path)
    -> Scope<Texture> {
  TextureProperties properties;
  properties.path = path;
  properties.format = ImageFormat::UNORM_RGBA8;
  properties.usage = ImageUsage::ColorAttachment | ImageUsage::TransferDst |
                     ImageUsage::TransferSrc;
  properties.layout = ImageLayout::ShaderReadOnlyOptimal;
  auto texture = Texture::construct(device, properties);
  return texture;
}

Texture::~Texture() {
  debug("Destroyed Texture '{}', size: {}", properties.identifier, cached_size);
}

Texture::Texture(const Device &dev, usize size, const Extent<u32> &extent)
    : device(&dev), data_buffer(size) {
  properties.extent = extent;
  properties.identifier = fmt::format("Empty-Size{}", size);
  data_buffer.fill_zero();
  cached_size = data_buffer.size();
  image = make_scope<Image>(
      *device,
      ImageProperties{
          .extent = properties.extent,
          .format = ImageFormat::UNORM_RGBA8,
          .tiling = ImageTiling::Linear,
          .usage = ImageUsage::Sampled | ImageUsage::Storage |
                   ImageUsage::TransferDst | ImageUsage::TransferSrc,
          .layout = ImageLayout::General,
          .min_filter = SamplerFilter::Linear,
          .max_filter = SamplerFilter::Linear,
          .address_mode = SamplerAddressMode::Repeat,
          .border_color = SamplerBorderColor::FloatOpaqueBlack,
      },
      data_buffer);
  debug("Created texture '{}', {}", properties.identifier, properties.extent);
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

  image = make_scope<Image>(*device,
                            ImageProperties{
                                .extent = properties.extent,
                                .format = properties.format,
                                .tiling = properties.tiling,
                                .usage = properties.usage,
                                .layout = properties.layout,
                                .min_filter = properties.min_filter,
                                .max_filter = properties.max_filter,
                                .address_mode = properties.address_mode,
                                .border_color = properties.border_color,
                            },
                            data_buffer);
  debug("Created texture '{}', {}", properties.identifier, properties.extent);
  cached_size = data_buffer.size();

  if (properties.data_retainment_strategy ==
      TextureDataRetainmentStrategy::Delete) {
    data_buffer.clear();
  }
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
