#pragma once

#include <memory>
#include <string_view>

namespace Core {

class DynamicLibraryLoader {
public:
  virtual ~DynamicLibraryLoader() = default;
  virtual auto get_symbol(std::string_view symbol) -> void * = 0;
  virtual bool is_valid() const = 0;

  static auto construct(std::string_view dll_name)
      -> std::unique_ptr<DynamicLibraryLoader>;
};

} // namespace Core