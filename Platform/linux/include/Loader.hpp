#pragma once

#include "DynamicLibraryLoader.hpp"

namespace Core::Linux {

class DynamicLibraryLoader : public Core::DynamicLibraryLoader {
public:
  DynamicLibraryLoader(std::string_view dll_name);
  ~DynamicLibraryLoader() override;

  void *get_symbol(std::string_view symbol_name) override;
  bool is_valid() const override;

private:
  void *lib_handle{nullptr};
};

} // namespace Core::Linux