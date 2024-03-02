#include "pch/vkgpgpu_pch.hpp"

#include "Texture.hpp"

#include "CommandBuffer.hpp"
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

auto Texture::construct_from_command_buffer(const Device &device,
                                            const TextureProperties &properties,

                                            CommandBuffer &command_buffer)
    -> Scope<Texture> {
  command_buffer.begin(0);
  auto constructed =
      Scope<Texture>(new Texture(device, properties, command_buffer));
  command_buffer.end_and_submit();
  return constructed;
}

auto Texture::construct_shader(const Device &device,
                               const TextureProperties &properties)
    -> Scope<Texture> {
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
  properties.usage = ImageUsage::ColourAttachment;
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

auto calculate_mip_count(const Extent<u32> &ext) -> u32 {
  auto max_dimension = std::max(ext.width, ext.height);
  return static_cast<u32>(std::floor(std::log2(max_dimension))) + 1;
}

auto determine_mip_count(const MipGeneration &mipGeneration,
                         const Extent<u32> &extent) -> u32 {
  return std::visit(overloaded{
                        [&ext = extent](MipGenerationStrategy strategy) -> u32 {
                          switch (strategy) {
                          case MipGenerationStrategy::FromSize:
                            return calculate_mip_count(ext);
                          case MipGenerationStrategy::Unused:
                          default:
                            return 1;
                          }
                        },
                        [](const LiteralMipData &data) -> u32 {
                          return data.mips > 0 ? data.mips : 1;
                        },
                    },
                    mipGeneration.strategy);
}

ResizeInfo determine_resize_info(const ResizeStrategy &strategy,
                                 const Extent<u32> &original_extent) {
  return std::visit(
      overloaded{
          [&](const ResizeMethod &method) -> ResizeInfo {
            switch (method) {
            case ResizeMethod::ByScalingFactor: {
              auto scale_factor =
                  std::get<ScalingFactorData>(strategy.strategy).scale_factor;
              auto scaled = original_extent.as<float>();
              scaled.width *= scale_factor;
              scaled.height *= scale_factor;
              return {
                  {
                      static_cast<u32>(scaled.width),
                      static_cast<u32>(scaled.height),
                  },
                  true,
              };
            }
            case ResizeMethod::ByAbsoluteSize: {
              auto size = std::get<Extent<u32>>(strategy.strategy);
              return {
                  size,
                  true,
              };
            }
            default:
              return {
                  original_extent,
                  false,
              };
            }
          },
          [&](const Extent<u32> &sizeData) -> ResizeInfo {
            return {{sizeData.width, sizeData.height}, true};
          },
          [&](const ScalingFactorData &scale_data) -> ResizeInfo {
            auto scaled = original_extent.as<float>();
            scaled.width *= scale_data.scale_factor;
            scaled.height *= scale_data.scale_factor;
            return {
                {
                    static_cast<u32>(scaled.width),
                    static_cast<u32>(scaled.height),
                },
                true,
            };
          }},
      strategy.strategy);
}

auto Texture::on_resize(const Extent<u32> &new_extent) -> void {
  if (new_extent == properties.extent)
    return;

  properties.extent = new_extent;
  data_buffer.clear();
  // Depends on the size of the format
  data_buffer.set_size_and_reallocate(properties.extent.size() * 4 *
                                      sizeof(float));
  data_buffer.fill_zero();
  cached_size = data_buffer.size();

  u32 mip_count =
      determine_mip_count(properties.mip_generation, properties.extent);

  image.reset();
  image = make_scope<Image>(*device,
                            ImageProperties{
                                .extent = properties.extent,
                                .mip_info =
                                    {
                                        .mips = mip_count,
                                        .use_mips = true,
                                    },
                                .generate_per_mip_image_views = mip_count > 1,
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
}

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
  if (properties.path.empty() && properties.identifier.empty()) {
    identifier = fmt::format("FromBuffer-Size{}", data_buffer.size());
  } else {
    identifier = properties.path.filename().string();
  }

  if (properties.identifier.empty()) {
    properties.identifier = identifier;
  }

  u32 mip_count =
      determine_mip_count(properties.mip_generation, properties.extent);

  image = make_scope<Image>(*device,
                            ImageProperties{
                                .extent = properties.extent,
                                .mip_info =
                                    {
                                        .mips = mip_count,
                                        .use_mips = true,
                                    },
                                .generate_per_mip_image_views = mip_count > 1,
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

Texture::Texture(const Device &dev, const TextureProperties &props,
                 CommandBuffer *buffer)
    : device(&dev), properties(props),
      data_buffer(load_databuffer_from_file(
          properties.path, properties.extent,
          determine_resize_info(properties.resize, properties.extent))) {
  if (!FS::exists(properties.path)) {
    throw NotFoundException(fmt::format("Texture file '{}' does not exist!",
                                        properties.path.string()));
  }

  properties.identifier = properties.path.filename().string();

  u32 mip_count =
      determine_mip_count(properties.mip_generation, properties.extent);

  const auto resize_info =
      determine_resize_info(properties.resize, properties.extent);

  ImageProperties image_properties{
      .extent = properties.extent,
      .mip_info =
          {
              .mips = mip_count,
              .use_mips = true,
          },
      .resize_info = resize_info,
      .generate_per_mip_image_views = mip_count > 1,
      .format = properties.format,
      .tiling = properties.tiling,
      .usage = properties.usage,
      .layout = properties.layout,
      .min_filter = properties.min_filter,
      .max_filter = properties.max_filter,
      .address_mode = properties.address_mode,
      .border_color = properties.border_color,
      .command_buffer_override = buffer,
  };

  image = make_scope<Image>(*device, image_properties, data_buffer);
  cached_size = data_buffer.size();
  debug("Created texture '{}', {} with size: {}", properties.identifier,
        properties.extent, human_readable_size(cached_size));

  if (properties.texture_data_strategy == TextureDataStrategy::Delete) {
    data_buffer.clear();
  }
}

Texture::Texture(const Device &dev, const TextureProperties &props,
                 CommandBuffer &command_buf)
    : Texture(dev, props) {
  if (!FS::exists(properties.path)) {
    throw NotFoundException(fmt::format("Texture file '{}' does not exist!",
                                        properties.path.string()));
  }

  properties.identifier = properties.path.filename().string();

  u32 mip_count =
      determine_mip_count(properties.mip_generation, properties.extent);

  const auto resize_info =
      determine_resize_info(properties.resize, properties.extent);

  ImageProperties image_properties{
      .extent = properties.extent,
      .mip_info =
          {
              .mips = mip_count,
              .use_mips = true,
          },
      .resize_info = resize_info,
      .generate_per_mip_image_views = mip_count > 1,
      .format = properties.format,
      .tiling = properties.tiling,
      .usage = properties.usage,
      .layout = properties.layout,
      .min_filter = properties.min_filter,
      .max_filter = properties.max_filter,
      .address_mode = properties.address_mode,
      .border_color = properties.border_color,
  };

  image = make_scope<Image>(*device, image_properties, data_buffer);
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
