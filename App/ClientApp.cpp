#include "ClientApp.hpp"

#include "BufferSet.hpp"
#include "Config.hpp"
#include "Material.hpp"

#include <array>
#include <imgui.h>
#include <vulkan/vulkan_core.h>

template <Core::Buffer::Type T>
auto create_or_get_write_descriptor_for(std::uint32_t frames_in_flight,
                                        Core::BufferSet<T> *buffer_set,
                                        Material &material)
    -> const std::vector<std::vector<VkWriteDescriptorSet>> & {
  static std::unordered_map<
      BufferSet<T> *,
      std::unordered_map<std::size_t,
                         std::vector<std::vector<VkWriteDescriptorSet>>>>
      buffer_set_write_descriptor_cache;

  constexpr auto get_descriptor_set_vector =
      [](auto &map, auto *key, auto hash) -> auto & { return map[key][hash]; };

  const auto &vulkan_shader = material.get_shader();
  const auto shader_hash = vulkan_shader.hash();
  if (buffer_set_write_descriptor_cache.contains(buffer_set)) {
    const auto &shader_map = buffer_set_write_descriptor_cache.at(buffer_set);
    if (shader_map.contains(shader_hash)) {
      const auto &write_descriptors = shader_map.at(shader_hash);
      return write_descriptors;
    }
  }

  if (!vulkan_shader.has_descriptor_set(0) ||
      !vulkan_shader.has_descriptor_set(1)) {
    return get_descriptor_set_vector(buffer_set_write_descriptor_cache,
                                     buffer_set, shader_hash);
  }

  const auto &shader_descriptor_sets =
      vulkan_shader.get_reflection_data().shader_descriptor_sets;
  if (shader_descriptor_sets.empty()) {
    return get_descriptor_set_vector(buffer_set_write_descriptor_cache,
                                     buffer_set, shader_hash);
  }

  const auto &shader_descriptor_set = shader_descriptor_sets[0];
  const auto &storage_buffers = shader_descriptor_set.storage_buffers;
  const auto &uniform_buffers = shader_descriptor_set.uniform_buffers;

  if constexpr (T == Buffer::Type::Storage) {
    for (const auto &binding : storage_buffers | std::views::keys) {
      auto &write_descriptors = get_descriptor_set_vector(
          buffer_set_write_descriptor_cache, buffer_set, shader_hash);
      write_descriptors.resize(frames_in_flight);
      for (auto frame = FrameIndex{0}; frame < frames_in_flight; ++frame) {
        const auto &stored_buffer = buffer_set->get(DescriptorBinding(binding),
                                                    frame, DescriptorSet(0));

        VkWriteDescriptorSet wds = {};
        wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wds.pBufferInfo = &stored_buffer->get_descriptor_info();
        wds.dstBinding = stored_buffer->get_binding();
        write_descriptors[frame].push_back(wds);
      }
    }
  } else {
    for (const auto &binding : uniform_buffers | std::views::keys) {
      auto &write_descriptors = get_descriptor_set_vector(
          buffer_set_write_descriptor_cache, buffer_set, shader_hash);
      write_descriptors.resize(frames_in_flight);
      for (auto frame = FrameIndex{0}; frame < frames_in_flight; ++frame) {
        const auto &stored_buffer = buffer_set->get(DescriptorBinding(binding),
                                                    frame, DescriptorSet(0));

        VkWriteDescriptorSet wds = {};
        wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wds.pBufferInfo = &stored_buffer->get_descriptor_info();
        wds.dstBinding = stored_buffer->get_binding();
        write_descriptors[frame].push_back(wds);
      }
    }
  }

  return get_descriptor_set_vector(buffer_set_write_descriptor_cache,
                                   buffer_set, shader_hash);
}

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

ClientApp::ClientApp(const ApplicationProperties &props)
    : App(props), uniform_buffer_set(*get_device()),
      storage_buffer_set(*get_device()), timer(*get_messaging_client()){};

auto ClientApp::on_update(floating ts) -> void {
  timer.begin();

  compute(ts);
  graphics(ts);

  timer.end();
}

void ClientApp::on_create() { perform(); }

void ClientApp::on_destroy() {
  // Destroy all fields
  command_buffer.reset();

  pipeline.reset();
  shader.reset();
  texture.reset();
  output_texture.reset();
}

auto ClientApp::graphics(floating ts) -> void {
  command_buffer->begin(frame());
  dispatcher->set_command_buffer(command_buffer.get());
  dispatcher->bind(*second_pipeline);

  auto &&[kernel_size, half_size, center_value] = compute_kernel_size<3>();
  second_material->set("pc.kernelSize", kernel_size);
  second_material->set("pc.halfSize", half_size);
  second_material->set("pc.precomputedCenterValue", center_value);
  second_material->set("input_image", *output_texture);
  second_material->set("output_image", *texture);
  update_material_for_rendering(FrameIndex{frame()}, *second_material,
                                &uniform_buffer_set, &storage_buffer_set);

  second_material->bind(*command_buffer, *second_pipeline, frame());

  constexpr auto wg_size = 16UL;

  // Number of groups in each dimension
  constexpr auto dispatchX = 1024 / wg_size;
  constexpr auto dispatchY = 1024 / wg_size;
  dispatcher->push_constant(*second_pipeline, *second_material);
  dispatcher->dispatch({
      .group_count_x = dispatchX,
      .group_count_y = dispatchY,
      .group_count_z = 1,
  });

  command_buffer->end_and_submit();
}

auto ClientApp::compute(floating ts) -> void {
  randomize_span_of_matrices(matrices);
  auto &input_buffer =
      storage_buffer_set.get(DescriptorBinding(0), frame(), DescriptorSet(0));
  input_buffer->write(matrices.data(), matrices.size() * sizeof(Math::Mat4));
  randomize_span_of_matrices(matrices);
  auto &other_input_buffer =
      storage_buffer_set.get(DescriptorBinding(1), frame(), DescriptorSet(0));
  other_input_buffer->write(matrices.data(),
                            matrices.size() * sizeof(Math::Mat4));

  static long double angle = 0.0;
  angle += ts * 0.1;

  // Map angle to 0 to 2pi
  const auto angle_in_radians = std::sin(angle);
  auto &simple_uniform =
      uniform_buffer_set.get(DescriptorBinding(3), frame(), DescriptorSet(0));
  simple_uniform->write(&angle_in_radians, simple_uniform->get_size());

  auto &&[kernel_size, half_size, center_value] = compute_kernel_size<3>();
  material->set("pc.kernelSize", kernel_size);
  material->set("pc.halfSize", half_size);
  material->set("pc.precomputedCenterValue", center_value);
  material->set("input_image", *texture);
  material->set("output_image", *output_texture);
  update_material_for_rendering(FrameIndex{frame()}, *material,
                                &uniform_buffer_set, &storage_buffer_set);

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
  pipeline->bind(*command_buffer);
  material->bind(*command_buffer, *pipeline, frame());

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
}

void ClientApp::perform() {
  texture = make_scope<Texture>(*get_device(), FS::texture("viking_room.png"));
  output_texture = Texture::empty_with_size(
      *get_device(), texture->size_bytes(), texture->get_extent());

  command_buffer = CommandBuffer::construct(
      *get_device(), CommandBufferProperties{
                         .queue_type = Queue::Type::Compute,
                         .record_stats = true,
                     });
  randomize_span_of_matrices(matrices);

  static constexpr auto matrices_size =
      std::array<Math::Mat4, 10>{}.size() * sizeof(Math::Mat4);

  storage_buffer_set.create(matrices_size, SetBinding(0));
  storage_buffer_set.create(matrices_size, SetBinding(1));
  storage_buffer_set.create(matrices_size, SetBinding(2));
  uniform_buffer_set.create(4, SetBinding(3));

  {
    shader = Shader::construct(*get_device(),
                               FS::shader("LaplaceEdgeDetection.comp.spv"));

    material = Material::construct(*get_device(), *shader);
    pipeline = make_scope<Pipeline>(*get_device(), PipelineConfiguration{
                                                       "LaplaceEdgeDetection",
                                                       PipelineStage::Compute,
                                                       *shader,
                                                   });
    auto &&[kernel_size, half_size, center_value] = compute_kernel_size<3>();
    material->set("pc.kernelSize", kernel_size);
    material->set("pc.halfSize", half_size);
    material->set("pc.precomputedCenterValue", center_value);
  }
  {
    second_shader = Shader::construct(
        *get_device(), FS::shader("LaplaceEdgeDetection_Second.comp.spv"));

    second_material = Material::construct(*get_device(), *second_shader);
    second_pipeline =
        make_scope<Pipeline>(*get_device(), PipelineConfiguration{
                                                "LaplaceEdgeDetection_Second",
                                                PipelineStage::Compute,
                                                *second_shader,
                                            });
    auto &&[kernel_size, half_size, center_value] = compute_kernel_size<127>();
    second_material->set("pc.kernelSize", kernel_size);
    second_material->set("pc.halfSize", half_size);
    second_material->set("pc.precomputedCenterValue", center_value);
  }

  dispatcher = make_scope<CommandDispatcher>(command_buffer.get());
}

void ClientApp::update_material_for_rendering(
    FrameIndex frame_index, Material &material_for_update,
    BufferSet<Buffer::Type::Uniform> *ubo_set,
    BufferSet<Buffer::Type::Storage> *sbo_set) {
  if (ubo_set != nullptr) {
    auto write_descriptors =
        create_or_get_write_descriptor_for<Buffer::Type::Uniform>(
            Config::frame_count, ubo_set, material_for_update);
    if (sbo_set != nullptr) {
      const auto &storage_buffer_write_sets =
          create_or_get_write_descriptor_for<Buffer::Type::Storage>(
              Config::frame_count, sbo_set, material_for_update);

      for (u32 frame = 0; frame < Config::frame_count; frame++) {
        const auto &sbo_ws = storage_buffer_write_sets[frame];
        const auto reserved_size =
            write_descriptors[frame].size() + sbo_ws.size();
        write_descriptors[frame].reserve(reserved_size);
        write_descriptors[frame].insert(write_descriptors[frame].end(),
                                        sbo_ws.begin(), sbo_ws.end());
      }
    }
    material_for_update.update_for_rendering(frame_index, write_descriptors);
  } else {
    material_for_update.update_for_rendering(frame_index);
  }
}

void ClientApp::on_interface(InterfaceSystem &) {
  /*
    // Create a fullscreen window for the dockspace
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    window_flags &= ~ImGuiWindowFlags_MenuBar;

    // Create the window that will contain the dockspace
    ImGui::Begin("DockSpace Demo", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // DockSpace
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
  */
  // Here you can add your own windows, for example:
  ImGui::Begin("FPS / Frametime");

  static constexpr auto draw_stats =
      [](const auto &average,
         const auto &command_buffer_with_compute_stats_available) {
        // Retrieve statistics from the timer and command buffer
        auto &&[frametime, fps] = average.get_statistics();
        auto &&[compute_times] =
            command_buffer_with_compute_stats_available.get_statistics();

        // Start a new ImGui table
        if (ImGui::BeginTable("StatsTable", 2)) {
          // Set up column headers
          ImGui::TableSetupColumn("Statistic");
          ImGui::TableSetupColumn("Value");
          ImGui::TableHeadersRow();

          // Row for frametime
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::Text("Frametime");
          ImGui::TableSetColumnIndex(1);
          ImGui::Text("%f ms", frametime);

          // Row for FPS
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::Text("FPS");
          ImGui::TableSetColumnIndex(1);
          ImGui::Text("%f", fps);

          // Row for Compute Time
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::Text("Compute Time");
          ImGui::TableSetColumnIndex(1);
          ImGui::Text("%f ms", compute_times);

          // End the table
          ImGui::EndTable();
        }
      };

  draw_stats(get_timer(), *command_buffer);

  ImGui::End();
  /*
    // Show the demo window (you can remove this once you're familiar with Dear
    // ImGui)
    ImGui::ShowDemoWindow();

    // End the dockspace window
    ImGui::End();
    */
}
