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
  properties.format = ImageFormat::SRGB_RGBA8;
  properties.usage =
      ImageUsage::TransferDst | ImageUsage::TransferSrc | ImageUsage::Sampled;
  properties.layout = ImageLayout::ShaderReadOnlyOptimal;
  auto texture = construct(device, properties);
  return texture;
}

auto Texture::construct_from_buffer(const Device &device,
                                    const TextureProperties &props,
                                    DataBuffer &&buffer) -> Scope<Texture> {
  auto texture = Scope<Texture>(new Texture(device, props, std::move(buffer)));
  return texture;
}

Texture::~Texture() {
  debug("Destroyed Texture '{}', size: {}", properties.identifier,
        human_readable_size(cached_size));
}

static auto calculate_mips(const Extent<u32> &ext) {
  auto max_dimension = std::max(ext.width, ext.height);
  return static_cast<u32>(std::ceil(std::log2(max_dimension))) + 1;
}

auto Texture::on_resize(const Extent<u32> &new_extent) -> void {}

Texture::Texture(const Device &dev, usize size, const Extent<u32> &extent)
    : device(&dev), data_buffer(size), storage(true) {
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
  debug("Created texture '{}', {} with size: {}", properties.identifier,
        properties.extent, human_readable_size(cached_size));
}

Texture::Texture(const Device &dev, const TextureProperties &props,
                 DataBuffer &&buffer)
    : device(&dev), properties(props), data_buffer(std::move(buffer)) {
  ensure(data_buffer.valid(), "DataBuffer must have size > 0");

  std::string identifier;
  if (properties.path.empty()) {
    identifier = fmt::format("FromBuffer-Size{}", data_buffer.size());
  } else {
    identifier = properties.path.filename().string();
  }

  u32 mip_count = 1;
  if (properties.mip_generation.strategy == MipGenerationStrategy::Unused) {
    mip_count = 1;
  } else if (properties.mip_generation.strategy ==
             MipGenerationStrategy::Literal) {
    mip_count =
        properties.mip_generation.mips > 0 ? properties.mip_generation.mips : 1;
  } else if (properties.mip_generation.strategy ==
             MipGenerationStrategy::FromSize) {
    mip_count = calculate_mips(properties.extent);
  }

  image = make_scope<Image>(*device,
                            ImageProperties{
                                .extent = properties.extent,
                                .mip_info =
                                    {
                                        .mips = mip_count,
                                        .use_mips = true,
                                    },
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

  debug("Created texture '{}', {} with size: {}", properties.identifier,
        properties.extent, human_readable_size(cached_size));
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

  u32 mip_count = 1;
  if (properties.mip_generation.strategy == MipGenerationStrategy::Unused) {
    mip_count = 1;
  } else if (properties.mip_generation.strategy ==
             MipGenerationStrategy::Literal) {
    mip_count =
        properties.mip_generation.mips > 0 ? properties.mip_generation.mips : 1;
  } else if (properties.mip_generation.strategy ==
             MipGenerationStrategy::FromSize) {
    mip_count = calculate_mips(properties.extent);
  }

  image = make_scope<Image>(*device,
                            ImageProperties{
                                .extent = properties.extent,
                                .mip_info =
                                    {
                                        .mips = mip_count,
                                        .use_mips = true,
                                    },
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
  cached_size = data_buffer.size();
  debug("Created texture '{}', {} with size: {}", properties.identifier,
        properties.extent, human_readable_size(cached_size));

  if (properties.texture_data_strategy == TextureDataStrategy::Delete) {
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

auto Texture::write_to_file(const FS::Path &path) const -> bool {
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
