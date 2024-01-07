#include "linux/include/Loader.hpp"

#include <dlfcn.h>

namespace Core::Linux {

DynamicLibraryLoader::DynamicLibraryLoader(std::string_view dll_name) {}

DynamicLibraryLoader::~DynamicLibraryLoader() {
  if (lib_handle) {
    dlclose(lib_handle);
  }
}

void *DynamicLibraryLoader::get_symbol(std::string_view symbolName) {
  return dlsym(lib_handle, symbolName.data());
}

bool DynamicLibraryLoader::is_valid() const { return lib_handle != nullptr; }

} // namespace Core::Linux