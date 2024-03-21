#pragma once

#include "Containers.hpp"
#include "IterationDecision.hpp"
#include "Types.hpp"

#include <filesystem>
#include <string>
#include <unordered_set>

namespace Core {

enum class FileChangeType : u8 { Created, Modified, Deleted };

struct FileInfo {
  std::filesystem::path path;
  FileChangeType change_type;
};

class IFilesystemChangeListener {
public:
  virtual ~IFilesystemChangeListener() = default;
  virtual auto get_file_extension_filter()
      -> const Container::StringLikeSet<std::string> & = 0;

  virtual auto on_file_created(const FileInfo &) -> IterationDecision {
    return IterationDecision::Break;
  };
  virtual auto on_file_modified(const FileInfo &) -> IterationDecision {
    return IterationDecision::Break;
  };
  virtual auto on_file_deleted(const FileInfo &) -> IterationDecision {
    return IterationDecision::Break;
  };
};

class FilesystemWatcher {
public:
  explicit FilesystemWatcher(const std::filesystem::path &);
  ~FilesystemWatcher();

  void add_change_listener(Scope<IFilesystemChangeListener> listener);

private:
  struct Impl;
  Scope<Impl> impl = nullptr;
};

} // namespace Core
