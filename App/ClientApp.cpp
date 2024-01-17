#include "ClientApp.hpp"

#include "BufferSet.hpp"

#include <array>

#ifdef _WIN32
static constexpr std::string_view renderdoc_dll_name = "renderdoc.dll";
#else
static constexpr std::string_view renderdoc_dll_name = "librenderdoc.so";
#endif

#if !defined(GPGPU_PIPELINE)
auto get_renderdoc_api(auto &device, auto &loader) -> RENDERDOC_API_1_6_0 * {
  DebugMarker::setup(device, device.get_physical_device());

  if (!loader->is_valid()) {
    info("Dynamic loader could not be constructed.");
    return nullptr;
  }

  auto RENDERDOC_GetAPI =
      std::bit_cast<pRENDERDOC_GetAPI>(loader->get_symbol("RENDERDOC_GetAPI"));
  RENDERDOC_API_1_1_2 *rdoc_api = nullptr;

  if (RENDERDOC_GetAPI &&
      RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api) == 1) {
    return rdoc_api;
  }

  info("Could not load symbols.");
  return nullptr;
}
#endif

auto randomize_span_of_matrices(std::span<Math::Mat4> matrices) -> void {
  static constexpr auto xy_to_index = [](Math::Mat4 &matrix, u64 x,
                                         u64 y) -> float & {
    return matrix.data.at(x).at(y);
  };

  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_real_distribution<float> dis(-1.0F, 1.0F);

  for (auto &matrix : matrices) {
    xy_to_index(matrix, 0, 0) = dis(gen);
    xy_to_index(matrix, 0, 1) = dis(gen);
    xy_to_index(matrix, 0, 2) = dis(gen);
    xy_to_index(matrix, 0, 3) = dis(gen);
    xy_to_index(matrix, 1, 0) = dis(gen);
    xy_to_index(matrix, 1, 1) = dis(gen);
    xy_to_index(matrix, 1, 2) = dis(gen);
    xy_to_index(matrix, 1, 3) = dis(gen);
    xy_to_index(matrix, 2, 0) = dis(gen);
    xy_to_index(matrix, 2, 1) = dis(gen);
    xy_to_index(matrix, 2, 2) = dis(gen);
    xy_to_index(matrix, 2, 3) = dis(gen);
    xy_to_index(matrix, 3, 0) = dis(gen);
    xy_to_index(matrix, 3, 1) = dis(gen);
    xy_to_index(matrix, 3, 2) = dis(gen);
    xy_to_index(matrix, 3, 3) = dis(gen);
  }
}

template <u32 N>
  requires(N % 2 == 1)
consteval auto compute_kernel_size() -> std::tuple<u32, u32, u32> {
  constexpr auto half_size = N / 2;
  constexpr auto kernel_size = N * N;
  constexpr auto center_value = N * N - 1;
  return {kernel_size, half_size, center_value};
}

auto ClientApp::on_update(double ts) -> void {
  compute(ts);
  graphics(ts);
}

void ClientApp::on_create() {

  loader = DynamicLibraryLoader::construct(renderdoc_dll_name);
#if !defined(GPGPU_PIPELINE)
  renderdoc = get_renderdoc_api(*get_device(), loader);
  if (renderdoc != nullptr) {
    renderdoc->SetCaptureTitle("Matrix Multiply");
  }
  perform();
#endif
}

void ClientApp::on_destroy() {
  // Destroy all fields
  command_buffer.reset();
  input_buffer.reset();
  other_input_buffer.reset();
  output_buffer.reset();
  simple_uniform.reset();
  descriptor_map.reset();

  pipeline.reset();
  shader.reset();
  texture.reset();
  output_texture.reset();

#if !defined(GPGPU_PIPELINE)
  if (renderdoc != nullptr) {
    renderdoc->RemoveHooks();
  }
#endif
}

auto ClientApp::graphics(double ts) -> void {}

auto ClientApp::compute(double ts) -> void {
  static auto begin_renderdoc = [&]() {
#if !defined(GPGPU_PIPELINE)
    if (renderdoc != nullptr) {
      renderdoc->StartFrameCapture(nullptr, nullptr);
    }
#endif
  };

  static auto end_renderdoc = [&]() {
#if !defined(GPGPU_PIPELINE)
    if (renderdoc != nullptr) {
      renderdoc->EndFrameCapture(nullptr, nullptr);
    }
#endif
  };

  Timer timer;

  begin_renderdoc();

  randomize_span_of_matrices(matrices);
  input_buffer->write(matrices.data(), matrices.size() * sizeof(Math::Mat4));
  randomize_span_of_matrices(matrices);
  other_input_buffer->write(matrices.data(),
                            matrices.size() * sizeof(Math::Mat4));

  static long double angle = 0.0;
  angle += ts * 0.1;

  // Map angle to 0 to 2pi
  const auto angle_in_radians = std::sin(angle);
  simple_uniform->write(&angle_in_radians, simple_uniform->get_size());

  auto &&[kernel_size, half_size, center_value] = compute_kernel_size<3>();
  material->set("pc.kernelSize", kernel_size);
  material->set("pc.halfSize", half_size);
  material->set("pc.precomputedCenterValue", center_value);

  // Begin command buffer
  command_buffer->begin(frame());
  // Bind pipeline
  DebugMarker::begin_region(command_buffer->get_command_buffer(),
                            "MatrixMultiply",
                            {
                                1.0F,
                                0.0F,
                                0.0F,
                            });
  dispatcher->bind(*pipeline);
  // Bind descriptor sets
  descriptor_map->bind(*command_buffer, frame(),
                       pipeline->get_pipeline_layout());
  // Dispatch

  constexpr auto wg_size = 16UL;

  // Number of groups in each dimension
  constexpr auto dispatchX = 1024 / wg_size;
  constexpr auto dispatchY = 1024 / wg_size;
  dispatcher->push_constant(*pipeline, *material);
  dispatcher->dispatch({
      .group_count_x = dispatchX,
      .group_count_y = dispatchY,
      .group_count_z = 1,
  });
  // End command buffer
  DebugMarker::end_region(command_buffer->get_command_buffer());
  command_buffer->end_and_submit();

  end_renderdoc();
}

void ClientApp::perform() {
  texture = make_scope<Texture>(*get_device(), FS::texture("viking_room.png"));
  output_texture = Texture::empty_with_size(
      *get_device(), texture->size_bytes(), texture->get_extent());

  command_buffer =
      make_scope<CommandBuffer>(*get_device(), CommandBuffer::Type::Compute);
  randomize_span_of_matrices(matrices);

  static constexpr auto matrices_size =
      std::array<Math::Mat4, 10>{}.size() * sizeof(Math::Mat4);
  input_buffer = make_scope<Buffer>(*get_device(), matrices_size,
                                    Buffer::Type::Storage, 0);
  other_input_buffer = make_scope<Buffer>(*get_device(), matrices_size,
                                          Buffer::Type::Storage, 1);
  output_buffer = make_scope<Buffer>(*get_device(), matrices_size,
                                     Buffer::Type::Storage, 2);

  struct Uniform {
    float whatever{};
  };
  simple_uniform = make_scope<Buffer>(*get_device(), sizeof(Uniform),
                                      Buffer::Type::Uniform, 3);

  BufferSet<Buffer::Type::Storage> storage_buffer_set{*get_device()};
  BufferSet<Buffer::Type::Uniform> uniform_buffer_set{*get_device()};
  storage_buffer_set.create(matrices_size, SetBinding(0));
  storage_buffer_set.create(matrices_size, SetBinding(1));
  storage_buffer_set.create(matrices_size, SetBinding(2));
  uniform_buffer_set.create(4, SetBinding(3));

  shader = make_scope<Shader>(*get_device(),
                              FS::shader("LaplaceEdgeDetection.comp.spv"));

  material = Material::construct(*get_device(), *shader);
  pipeline = make_scope<Pipeline>(*get_device(), PipelineConfiguration{
                                                     "LaplaceEdgeDetection",
                                                     PipelineStage::Compute,
                                                     *shader,
                                                 });

  descriptor_map = make_scope<DescriptorMap>(
      *get_device(), *get_descriptor_resource(), *shader);
  descriptor_map->add_for_frames(0, *input_buffer);
  descriptor_map->add_for_frames(1, *input_buffer);
  descriptor_map->add_for_frames(2, *output_buffer);
  descriptor_map->add_for_frames(3, *simple_uniform);

  descriptor_map->add_for_frames(0, *texture);
  descriptor_map->add_for_frames(1, *output_texture);

  dispatcher = make_scope<CommandDispatcher>(command_buffer.get());

  auto &&[kernel_size, half_size, center_value] = compute_kernel_size<3>();
  material->set("pc.kernelSize", kernel_size);
  material->set("pc.halfSize", half_size);
  material->set("pc.precomputedCenterValue", center_value);
}
