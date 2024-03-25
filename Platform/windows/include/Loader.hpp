#pragma once

#include "DynamicLibraryLoader.hpp"

#include <Windows.h>

namespace Core::Windows {

class DynamicLibraryLoader : public Core::DynamicLibraryLoader {
public:
  DynamicLibraryLoader(std::string_view dll_name);
  ~DynamicLibraryLoader() override;

  void *get_symbol(std::string_view symbol_name) override;
  bool is_valid() const override;

private:
  HMODULE lib_handle{nullptr};
};

} // namespace Core::Windows
