#pragma once

#include "Allocator.hpp"
#include "App.hpp"
#include "Buffer.hpp"
#include "BufferSet.hpp"
#include "CommandBuffer.hpp"
#include "CommandDispatcher.hpp"
#include "DebugMarker.hpp"
#include "DescriptorMap.hpp"
#include "DescriptorResource.hpp"
#include "DynamicLibraryLoader.hpp"
#include "Environment.hpp"
#include "Filesystem.hpp"
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

#if !defined(GPGPU_PIPELINE)
#include <renderdoc_app.h>
#endif

using namespace Core;

class ClientApp : public App {
public:
  explicit ClientApp(const ApplicationProperties &props);
  ~ClientApp() override = default;

  void on_update(floating ts) override;
  void on_create() override;
  void on_destroy() override;

private:
  Scope<DescriptorMap> descriptor_map;
  Scope<CommandDispatcher> dispatcher;
  Scope<DynamicLibraryLoader> loader;

  using UniformSet = BufferSet<Buffer::Type::Uniform>;
  using StorageSet = BufferSet<Buffer::Type::Storage>;

  UniformSet uniform_buffer_set;
  StorageSet storage_buffer_set;

  Timer timer;

  Scope<CommandBuffer> command_buffer;
  Scope<Material> material;

  Scope<Pipeline> pipeline;
  Scope<Shader> shader;
  Scope<Texture> texture;
  Scope<Texture> output_texture;

  std::array<Math::Mat4, 10> matrices{};
  std::array<Math::Mat4, 10> output_matrices{};

#if !defined(GPGPU_PIPELINE)
  RENDERDOC_API_1_6_0 *renderdoc{nullptr};
#endif

  auto compute(floating ts) -> void;
  auto graphics(floating ts) -> void;

  void perform();
  auto update_material_for_rendering(FrameIndex frame_index, Material &material,
                                     BufferSet<Buffer::Type::Uniform> *ubo_set,
                                     BufferSet<Buffer::Type::Storage> *sbo_set)
      -> void;
};
