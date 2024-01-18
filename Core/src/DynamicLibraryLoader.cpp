#include "pch/vkgpgpu_pch.hpp"

#include "DynamicLibraryLoader.hpp"

#include <memory>

#ifdef _WIN32
#include "windows/include/Loader.hpp"
#else
#include "linux/include/Loader.hpp"
#endif

namespace Core {

auto DynamicLibraryLoader::construct(std::string_view dll_name)
    -> std::unique_ptr<DynamicLibraryLoader> {
#ifdef _WIN32
  return std::make_unique<Windows::DynamicLibraryLoader>(dll_name);
#else
  return std::make_unique<Linux::DynamicLibraryLoader>(dll_name);
#endif
}

} // namespace Core
