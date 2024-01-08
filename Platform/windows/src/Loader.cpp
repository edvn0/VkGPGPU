#include "windows/include/Loader.hpp"

#include <Windows.h>

namespace Core::Windows {

DynamicLibraryLoader::DynamicLibraryLoader(std::string_view dll_name) {
  lib_handle = LoadLibrary(dll_name.data());
}

DynamicLibraryLoader::~DynamicLibraryLoader() {
  if (lib_handle) {
    FreeLibrary(static_cast<HMODULE>(lib_handle));
  }
}

void *DynamicLibraryLoader::get_symbol(std::string_view symbolName) {
  return GetProcAddress(static_cast<HMODULE>(lib_handle), symbolName.data());
}

bool DynamicLibraryLoader::is_valid() const { return lib_handle != nullptr; }

} // namespace Core::Windows