#pragma once

#include "Allocator.hpp"
#include "App.hpp"
#include "Buffer.hpp"
#include "BufferSet.hpp"
#include "CommandBuffer.hpp"
#include "CommandDispatcher.hpp"
#include "DebugMarker.hpp"
#include "DescriptorResource.hpp"
#include "DynamicLibraryLoader.hpp"
#include "Environment.hpp"
#include "Filesystem.hpp"
#include "Framebuffer.hpp"
#include "Image.hpp"
#include "Instance.hpp"
#include "Logger.hpp"
#include "Material.hpp"
#include "Math.hpp"
#include "Pipeline.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Timer.hpp"
#include "Types.hpp"

#include <numbers>
#include <random>

#include "bus/MessagingClient.hpp"
#include "widgets/Widget.hpp"

using namespace Core;

class ClientApp : public App {
public:
  explicit ClientApp(const ApplicationProperties &props);
  ~ClientApp() override = default;

  void on_update(floating ts) override;
  void on_resize(const Extent<u32> &) override;
  void on_interface(Core::InterfaceSystem &) override;
  void on_create() override;
  void on_destroy() override;

private:
  Scope<CommandDispatcher> dispatcher;
  Scope<DynamicLibraryLoader> loader;

  std::vector<Scope<Widget>> widgets{};

  using UniformSet = BufferSet<Buffer::Type::Uniform>;
  UniformSet uniform_buffer_set;
  using StorageSet = BufferSet<Buffer::Type::Storage>;
  StorageSet storage_buffer_set;

  Timer timer;

  Scope<CommandBuffer> command_buffer;
  Scope<CommandBuffer> graphics_command_buffer;
  Scope<Framebuffer> framebuffer;

  Scope<Material> material;
  Scope<Pipeline> pipeline;
  Scope<Shader> shader;

  Scope<Material> second_material;
  Scope<Pipeline> second_pipeline;
  Scope<Shader> second_shader;

  Scope<Material> graphics_material;
  Scope<GraphicsPipeline> graphics_pipeline;
  Scope<Shader> graphics_shader;

  Scope<Texture> texture;
  Scope<Texture> output_texture;
  Scope<Texture> output_texture_second;

  struct PCForMaterial {
    i32 kernel_size{};
    i32 half_size{};
    i32 center_value{};
  };
  PCForMaterial pc{};

  std::array<Math::Mat4, 10> matrices{};

  auto compute(floating ts) -> void;
  auto graphics(floating ts) -> void;

  void perform();
  static auto update_material_for_rendering(
      FrameIndex frame_index, Material &material_for_update,
      BufferSet<Buffer::Type::Uniform> *ubo_set,
      BufferSet<Buffer::Type::Storage> *sbo_set) -> void;
};
