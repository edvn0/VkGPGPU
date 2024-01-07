#pragma once

#include "DynamicLibraryLoader.hpp"

namespace Core::Windows {

class DynamicLibraryLoader : public Core::DynamicLibraryLoader {
public:
  using Core::DynamicLibraryLoader::DynamicLibraryLoader;
  ~DynamicLibraryLoader() override = default;

private:
  auto load_dll() -> void;
  void *dll{nullptr};
};

} // namespace Core::Windows