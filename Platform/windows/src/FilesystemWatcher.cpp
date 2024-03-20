#include "Containers.hpp"
#include "Ensure.hpp"
#include "FilesystemListener.hpp"
#include "Formatters.hpp"

#include <Windows.h>
#include <array>
#include <atomic>
#include <bit>
#include <magic_enum.hpp>
#include <thread>

namespace Core {

class FilesystemWatcher::Impl {
public:
  explicit Impl(const std::string &directory) : dir_path(directory) {
    start_watching();
  }
  ~Impl() = default;

  void add_change_listener(Scope<IFilesystemChangeListener> &&listener) {
    change_listeners.push_back(std::move(listener));
  }

  void start_watching() {
    monitor_thread = std::jthread(
        [this](const auto &stop_token) { monitor_directory(stop_token); });
  }

private:
  std::string dir_path;
  std::vector<Scope<IFilesystemChangeListener>> change_listeners;
  std::jthread monitor_thread;

  void monitor_directory(const std::stop_token &);
  auto notify_listeners(const FileInfo &info) {
    warn("[FilesystemWatcher] FileInfo: {}, Type: {}", info.path,
         magic_enum::enum_name(info.change_type));
    Container::for_each(
        change_listeners, [&fi = info](Scope<IFilesystemChangeListener> &ptr) {
          if (!ptr->get_file_extension_filter().contains(
                  fi.path.extension().string())) {
            return IterationDecision::Continue;
          }

          IterationDecision decision{IterationDecision::Continue};
          if (fi.change_type == FileChangeType::Created) {
            decision = ptr->on_file_created(fi);
          }
          if (fi.change_type == FileChangeType::Modified) {
            decision = ptr->on_file_modified(fi);
          }
          if (fi.change_type == FileChangeType::Deleted) {
            decision = ptr->on_file_deleted(fi);
          }

          return decision;
        });
  }

  static auto close_handle(HANDLE handle) -> void {
    if (handle) {
      CloseHandle(handle);
    }
  }
};

void FilesystemWatcher::Impl::monitor_directory(
    const std::stop_token &stop_token) {
  constexpr DWORD buffer_length = 10 * 1024;
  std::array<BYTE, buffer_length> buffer{};
  DWORD bytes_returned;

  auto dir_handle = std::unique_ptr<void, decltype(&Impl::close_handle)>(
      CreateFileA(dir_path.c_str(), FILE_LIST_DIRECTORY,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING,
                  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr),
      &Impl::close_handle);

  if (dir_handle.get() == INVALID_HANDLE_VALUE) {
    return;
  }

  OVERLAPPED overlapped = {};
  auto event_handle = std::unique_ptr<void, decltype(&Impl::close_handle)>(
      CreateEvent(nullptr, FALSE, FALSE, nullptr), &Impl::close_handle);

  if (!event_handle.get()) {
    return;
  }

  overlapped.hEvent = event_handle.get();

  auto cleanup = [&]() {};

  while (!stop_token.stop_requested()) {
    DWORD dw_bytes_returned = 0;
    BOOL result = ReadDirectoryChangesW(
        dir_handle.get(), buffer.data(), buffer_length, TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
        &dw_bytes_returned, &overlapped, nullptr);

    if (!result) {
      cleanup();
      return;
    }

    DWORD wait_status = WaitForSingleObject(overlapped.hEvent, 100);

    if (wait_status == WAIT_OBJECT_0) {
      DWORD next_entry_offset = 0;
      do {
        FILE_NOTIFY_INFORMATION *notifyInfo =
            reinterpret_cast<FILE_NOTIFY_INFORMATION *>(buffer.data() +
                                                        next_entry_offset);
        FileInfo fileInfo;

        fileInfo.path = std::wstring(
            notifyInfo->FileName, notifyInfo->FileNameLength / sizeof(WCHAR));

        switch (notifyInfo->Action) {
        case FILE_ACTION_ADDED:
          fileInfo.change_type = FileChangeType::Created;
          break;
        case FILE_ACTION_REMOVED:
          fileInfo.change_type = FileChangeType::Deleted;
          break;
        case FILE_ACTION_MODIFIED:
          fileInfo.change_type = FileChangeType::Modified;
          break;
        default:
          break;
        }

        notify_listeners(fileInfo);

        next_entry_offset = notifyInfo->NextEntryOffset;
      } while (next_entry_offset != 0);

      ResetEvent(overlapped.hEvent);
    } else if (wait_status == WAIT_TIMEOUT) {
      continue;
    } else {
      break;
    }
  }

  cleanup();
}

FilesystemWatcher::FilesystemWatcher(const std::filesystem::path &directory) {
  impl = make_scope<FilesystemWatcher::Impl>(directory.string());
}

FilesystemWatcher::~FilesystemWatcher() { impl.reset(); }

void FilesystemWatcher::add_change_listener(
    Scope<IFilesystemChangeListener> listener) {
  impl->add_change_listener(std::move(listener));
}

} // namespace Core
