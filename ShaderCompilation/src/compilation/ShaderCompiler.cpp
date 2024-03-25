#include "compilation/ShaderCompiler.hpp"

#include "Exception.hpp"
#include "Shader.hpp"

#include <fstream>
#include <istream>
#include <shaderc/shaderc.hpp>

namespace Compilation {

class FileCouldNotBeOpened : public Core::BaseException {
public:
  using Core::BaseException::BaseException;
};

ShaderCompiler::~ShaderCompiler() = default;

namespace {
auto preprocess_shader(shaderc::Compiler &compiler,
                       shaderc::CompileOptions &options,
                       const std::string &source_name, shaderc_shader_kind kind,
                       const std::string &source) -> std::string {
  shaderc::PreprocessedSourceCompilationResult result =
      compiler.PreprocessGlsl(source, kind, source_name.c_str(), options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    error("Failed to preprocess shader: {}", result.GetErrorMessage());
    return {};
  }

  return {result.cbegin(), result.cend()};
}

auto compile_shader(shaderc::Compiler &compiler,
                    shaderc::CompileOptions &options,
                    const std::string &source_name, shaderc_shader_kind kind,
                    const std::string &source) -> std::vector<uint32_t> {
  shaderc::SpvCompilationResult result =
      compiler.CompileGlslToSpv(source, kind, source_name.c_str(), options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    error("Failed to compile shader: {}", result.GetErrorMessage());
    return {};
  }

  return {result.cbegin(), result.cend()};
}

auto read_file(const std::filesystem::path &path) -> std::string {
  // Convert to an absolute path and open the file
  const auto &absolute_path = absolute(path);
  const std::ifstream file(absolute_path, std::ios::in | std::ios::binary);

  // Check if the file was successfully opened
  if (!file.is_open()) {
    error("Failed to open file: {}", absolute_path.string());
    throw FileCouldNotBeOpened("Failed to open file: " +
                               absolute_path.string());
  }

  // Use a std::stringstream to read the file's contents into a string
  std::stringstream buffer;
  buffer << file.rdbuf();

  // Return the contents of the file as a string
  return buffer.str();
}
} // namespace

struct ShaderIncluder : public shaderc::CompileOptions::IncluderInterface {
  std::unordered_map<std::string, std::string> files;

  ShaderIncluder(const std::vector<std::filesystem::path> &includes,
                 const Core::Container::StringLikeMap<std::string> &macros)
      : include_directories(includes), macro_definitions(macros) {}

  ~ShaderIncluder() = default;
  shaderc_include_result *GetInclude(const char *requested_source,
                                     shaderc_include_type type,
                                     const char *requesting_source,
                                     size_t include_depth) override {
    // Find the requested source file
    const std::filesystem::path requested_path = requested_source;
    const auto absolute_requested_path = Core::FS::shader_directory() /
                                         Core::FS::Path{"include"} /
                                         requested_path;

    // Check if the requested source file exists
    if (!Core::FS::exists(absolute_requested_path)) {
      error("Failed to find include file: {}", absolute_requested_path);
      return nullptr;
    }

    // Read the requested source file
    const std::string absolute_path_as_string =
        absolute_requested_path.string();
    const std::string source = read_file(absolute_requested_path);

    files[absolute_path_as_string] = source;

    auto *result = new shaderc_include_result;
    result->source_name = absolute_path_as_string.c_str();
    result->source_name_length = absolute_path_as_string.size();
    result->content = files.at(absolute_path_as_string).c_str();
    result->content_length = files.at(absolute_path_as_string).size();
    result->user_data = nullptr;

    return result;
  }

  void ReleaseInclude(shaderc_include_result *data) override { delete data; }

  std::vector<std::filesystem::path> include_directories;
  Core::Container::StringLikeMap<std::string> macro_definitions;
};

struct ShaderCompiler::Impl {
  // Spirv cross GLSL compiler
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
};

auto ShaderCompiler::compile_graphics(
    const std::filesystem::path &vertex_shader_path,
    const std::filesystem::path &fragment_shader_path)
    -> Core::Ref<Core::Shader> {

  // Read shader source files
  std::string vertex_shader_source = read_file(vertex_shader_path);
  std::string fragment_shader_source = read_file(fragment_shader_path);

  // Preprocess and compile vertex shader
  auto preprocessed_vertex_shader = preprocess_shader(
      impl->compiler, impl->options, vertex_shader_path.string(),
      shaderc_glsl_vertex_shader, vertex_shader_source);
  auto compiled_vertex_shader =
      compile_shader(impl->compiler, impl->options, vertex_shader_path.string(),
                     shaderc_glsl_vertex_shader, preprocessed_vertex_shader);

  // Preprocess and compile fragment shader
  auto preprocessed_fragment_shader = preprocess_shader(
      impl->compiler, impl->options, fragment_shader_path.string(),
      shaderc_glsl_fragment_shader, fragment_shader_source);
  auto compiled_fragment_shader = compile_shader(
      impl->compiler, impl->options, fragment_shader_path.string(),
      shaderc_glsl_fragment_shader, preprocessed_fragment_shader);

  if (compiled_vertex_shader.empty() || compiled_fragment_shader.empty() ||
      preprocessed_vertex_shader.empty() ||
      preprocessed_fragment_shader.empty()) {
    return nullptr;
  }

  std::unordered_map<Core::Shader::Type, std::vector<Core::u32>>
      compiled_spirv_per_stage{
          {
              Core::Shader::Type::Vertex,
              compiled_vertex_shader,
          },
          {
              Core::Shader::Type::Fragment,
              compiled_fragment_shader,
          },
      };
  auto name = vertex_shader_path.filename().replace_extension().string();

  return make_ref<Core::Shader>(*device, std::move(compiled_spirv_per_stage),
                                name);
}

auto ShaderCompiler::compile_compute(
    const std::filesystem::path &compute_shader_path)
    -> Core::Ref<Core::Shader> {

  // Read shader source file
  std::string compute_shader_source = read_file(compute_shader_path);

  // Preprocess and compile compute shader
  auto preprocessed_compute_shader = preprocess_shader(
      impl->compiler, impl->options, compute_shader_path.string(),
      shaderc_glsl_compute_shader, compute_shader_source);
  auto compiled_compute_shader = compile_shader(
      impl->compiler, impl->options, compute_shader_path.string(),
      shaderc_glsl_compute_shader, preprocessed_compute_shader);

  if (compiled_compute_shader.empty() || preprocessed_compute_shader.empty()) {
    return nullptr;
  }

  std::unordered_map<Core::Shader::Type, std::vector<Core::u32>>
      compiled_spirv_per_stage{
          {
              Core::Shader::Type::Compute,
              compiled_compute_shader,
          },
      };

  auto name = compute_shader_path.filename().replace_extension().string();

  return make_ref<Core::Shader>(*device, std::move(compiled_spirv_per_stage),
                                name);
}

auto ShaderCompiler::compile_graphics_scoped(
    const std::filesystem::path &vertex_shader_path,
    const std::filesystem::path &fragment_shader_path)
    -> Core::Scope<Core::Shader> {

  // Read shader source files
  std::string vertex_shader_source = read_file(vertex_shader_path);
  std::string fragment_shader_source = read_file(fragment_shader_path);

  // Preprocess and compile vertex shader
  auto preprocessed_vertex_shader = preprocess_shader(
      impl->compiler, impl->options, vertex_shader_path.string(),
      shaderc_glsl_vertex_shader, vertex_shader_source);
  auto compiled_vertex_shader =
      compile_shader(impl->compiler, impl->options, vertex_shader_path.string(),
                     shaderc_glsl_vertex_shader, preprocessed_vertex_shader);

  // Preprocess and compile fragment shader
  auto preprocessed_fragment_shader = preprocess_shader(
      impl->compiler, impl->options, fragment_shader_path.string(),
      shaderc_glsl_fragment_shader, fragment_shader_source);
  auto compiled_fragment_shader = compile_shader(
      impl->compiler, impl->options, fragment_shader_path.string(),
      shaderc_glsl_fragment_shader, preprocessed_fragment_shader);

  if (compiled_vertex_shader.empty() || compiled_fragment_shader.empty() ||
      preprocessed_vertex_shader.empty() ||
      preprocessed_fragment_shader.empty()) {
    return nullptr;
  }

  std::unordered_map<Core::Shader::Type, std::vector<Core::u32>>
      compiled_spirv_per_stage{
          {
              Core::Shader::Type::Vertex,
              compiled_vertex_shader,
          },
          {
              Core::Shader::Type::Fragment,
              compiled_fragment_shader,
          },
      };

  auto name = vertex_shader_path.filename().replace_extension().string();

  return make_scope<Core::Shader>(*device, std::move(compiled_spirv_per_stage),
                                  name);
}

auto ShaderCompiler::compile_compute_scoped(
    const std::filesystem::path &compute_shader_path)
    -> Core::Scope<Core::Shader> {

  // Read shader source file
  std::string compute_shader_source = read_file(compute_shader_path);

  // Preprocess and compile compute shader
  auto preprocessed_compute_shader = preprocess_shader(
      impl->compiler, impl->options, compute_shader_path.string(),
      shaderc_glsl_compute_shader, compute_shader_source);
  auto compiled_compute_shader = compile_shader(
      impl->compiler, impl->options, compute_shader_path.string(),
      shaderc_glsl_compute_shader, preprocessed_compute_shader);

  if (compiled_compute_shader.empty() || preprocessed_compute_shader.empty()) {
    return nullptr;
  }

  std::unordered_map<Core::Shader::Type, std::vector<Core::u32>>
      compiled_spirv_per_stage{
          {
              Core::Shader::Type::Compute,
              compiled_compute_shader,
          },
      };
  auto name = compute_shader_path.filename().replace_extension().string();

  return make_scope<Core::Shader>(*device, std::move(compiled_spirv_per_stage),
                                  name);
}

ShaderCompiler::ShaderCompiler(const Core::Device &dev,
                               const ShaderCompilerConfiguration &conf)
    : device(&dev), configuration(conf) {
  impl = Core::make_scope<Impl>();
  static constexpr auto to_shaderc_optimization_level = [](Core::u32 level) {
    switch (level) {
    case 0:
      return shaderc_optimization_level_zero;
    case 1:
      return shaderc_optimization_level_size;
    case 2:
      return shaderc_optimization_level_performance;
    default:
      return shaderc_optimization_level_zero;
    }
  };
  impl->options.SetOptimizationLevel(
      to_shaderc_optimization_level(configuration.optimisation_level));
  if (configuration.debug_information_level == DebugInformationLevel::Full) {
    impl->options.SetGenerateDebugInfo();
  }
  // VUlkan 1.3 env
  impl->options.SetTargetEnvironment(shaderc_target_env_vulkan,
                                     shaderc_env_version_vulkan_1_3);
  if (configuration.warnings_as_errors) {
    impl->options.SetWarningsAsErrors();
  }
  impl->options.SetInvertY(true);
  impl->options.SetTargetSpirv(shaderc_spirv_version_1_6);
  impl->options.SetSourceLanguage(shaderc_source_language_glsl);
  impl->options.SetForcedVersionProfile(460, shaderc_profile_none);
  impl->options.SetPreserveBindings(true);

  impl->options.SetIncluder(std::make_unique<ShaderIncluder>(
      configuration.include_directories, configuration.macro_definitions));
}

} // namespace Compilation
