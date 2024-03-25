#include "App.hpp"
#ifdef GPGPU_ENTRY

#include "Allocator.hpp"
#include "Environment.hpp"
#include "Filesystem.hpp"
#include "Logger.hpp"

using namespace Core;

int main(int argc, char **argv) {
  const std::span args(argv, argc);
  std::vector<std::string_view> views{};
  for (const auto &arg : args) {
    views.emplace_back(arg);
  }

  // Set current path to "--wd" argument of views, if there is one
  if (auto wd = std::ranges::find(views, "--wd");
      wd != views.end() && ++wd != views.end()) {
    FS::set_current_path(*wd);
  }

#ifdef GPGPU_CUSTOM_BASE_PATH
  FS::set_current_path(FS::Path{GPGPU_CUSTOM_BASE_PATH});
#endif

  std::array<std::string, 2> keys{"LOG_LEVEL", "ENABLE_VALIDATION_LAYERS"};
  Environment::initialize(keys);

  ApplicationProperties props{
      .headless = false,
      .start_fullscreen = false,
  };

  {
    auto application = make_application(props);
    application->run();
  }

  Logger::stop();

  // Close dll
  info("Exiting");
  return 0;
}

#else
#error You need to define 'GPGPU_ENTRY' before including this file.
#endif
