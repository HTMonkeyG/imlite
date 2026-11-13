#include "LitePlatform.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <thread>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#else
  #include <unistd.h>
#endif

#if defined(__APPLE__)
  #include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;


namespace {
/* Lua 5.2 opens files through the C runtime, which on Windows interprets
** narrow paths in the active ANSI code page. All paths exchanged with Lua are
** therefore kept in that native narrow form, exactly like the original
** bindings did; only the clipboard and window titles are UTF-8.
**
** std::filesystem insists on valid UTF-8 for narrow paths on Windows, so the
** native form is translated to UTF-16 before it reaches the filesystem. */
#if defined(_WIN32)
std::wstring NativeToWide(const std::string &text) {
  if (text.empty()) {
    return std::wstring();
  }
  int length = MultiByteToWideChar(
    CP_ACP, 0, text.c_str(), (int) text.size(), nullptr, 0);
  if (length <= 0) {
    return std::wstring();
  }
  std::wstring result((size_t) length, L'\0');
  MultiByteToWideChar(
    CP_ACP, 0, text.c_str(), (int) text.size(), &result[0], length);
  return result;
}

std::string WideToNative(const std::wstring &text) {
  if (text.empty()) {
    return std::string();
  }
  int length = WideCharToMultiByte(
    CP_ACP, 0, text.c_str(), (int) text.size(), nullptr, 0, nullptr, nullptr);
  if (length <= 0) {
    return std::string();
  }
  std::string result((size_t) length, '\0');
  WideCharToMultiByte(
    CP_ACP, 0, text.c_str(), (int) text.size(), &result[0], length,
    nullptr, nullptr);
  return result;
}

fs::path ToPath(const char *path) {
  return fs::path(NativeToWide(path ? path : ""));
}

std::string FromPath(const fs::path &path) {
  return WideToNative(path.wstring());
}
#else
fs::path ToPath(const char *path) {
  return fs::path(path ? path : "");
}

std::string FromPath(const fs::path &path) {
  return path.string();
}
#endif

/* std::filesystem::file_time_type uses an implementation defined clock; the
** usual C++17 trick translates it to the system clock. */
double FileTimeToUnixSeconds(fs::file_time_type time) {
  auto systemTime =
    std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
  return std::chrono::duration<double>(systemTime.time_since_epoch()).count();
}
}


namespace LitePlatform {

std::string GetExecutablePath() {
#if defined(_WIN32)
  char buffer[4096];
  DWORD length = GetModuleFileNameA(nullptr, buffer, (DWORD) sizeof(buffer));
  if (length == 0 || (size_t) length >= sizeof(buffer)) {
    return std::string();
  }
  return std::string(buffer, length);

#elif defined(__linux__)
  char buffer[4096];
  ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (length <= 0) {
    return std::string();
  }
  buffer[length] = '\0';
  return std::string(buffer);

#elif defined(__APPLE__)
  char buffer[4096];
  uint32_t size = (uint32_t) sizeof(buffer);
  if (_NSGetExecutablePath(buffer, &size) != 0) {
    return std::string();
  }
  return std::string(buffer);

#else
  return std::string();
#endif
}


const char *GetName() {
#if defined(_WIN32)
  return "Windows";
#elif defined(__APPLE__)
  return "Mac OS X";
#elif defined(__linux__)
  return "Linux";
#else
  return "Unknown";
#endif
}


double GetTime() {
  static const auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(now - start).count();
}


void Sleep(double seconds) {
  if (seconds <= 0.0) {
    return;
  }
  std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}


bool ListDirectory(
  const char *path,
  std::vector<std::string> &entries,
  std::string &error
) {
  entries.clear();
  std::error_code ec;
  fs::directory_iterator iterator(
    ToPath(path), fs::directory_options::skip_permission_denied, ec);
  if (ec) {
    error = ec.message();
    return false;
  }

  /* Incrementing a directory iterator can throw if an entry disappears while
  ** we walk the directory. */
  try {
    for (const fs::directory_entry &entry : iterator) {
      entries.push_back(FromPath(entry.path().filename()));
    }
  } catch (const fs::filesystem_error &exception) {
    error = exception.what();
    return false;
  }
  return true;
}


bool GetFileInfo(
  const char *path,
  std::string &type,
  long long &size,
  double &modified,
  std::string &error
) {
  fs::path filePath = ToPath(path);
  std::error_code ec;
  fs::file_status status = fs::status(filePath, ec);
  if (ec || status.type() == fs::file_type::not_found) {
    error = ec ? ec.message() : "No such file or directory";
    return false;
  }

  type.clear();
  size = 0;
  modified = 0.0;

  if (fs::is_regular_file(status)) {
    type = "file";
    std::error_code sizeEc;
    uintmax_t fileSize = fs::file_size(filePath, sizeEc);
    if (!sizeEc) {
      size = (long long) fileSize;
    }
  } else if (fs::is_directory(status)) {
    type = "dir";
  }

  std::error_code timeEc;
  fs::file_time_type writeTime = fs::last_write_time(filePath, timeEc);
  if (!timeEc) {
    modified = FileTimeToUnixSeconds(writeTime);
  }

  return true;
}


bool GetAbsolutePath(const char *path, std::string &result) {
  std::error_code ec;
  fs::path absolute = fs::absolute(ToPath(path), ec);
  if (ec) {
    return false;
  }
  result = FromPath(absolute.lexically_normal());
  return true;
}


bool ChangeDirectory(const char *path, std::string &error) {
  std::error_code ec;
  fs::current_path(ToPath(path), ec);
  if (ec) {
    error = ec.message();
    return false;
  }
  return true;
}


void Execute(const char *command) {
  if (!command) {
    return;
  }
#if defined(_WIN32)
  std::string line = "cmd /c \"";
  line += command;
  line += "\"";
  WinExec(line.c_str(), SW_HIDE);
#else
  std::string line = command;
  line += " &";
  int result = std::system(line.c_str());
  (void) result;
#endif
}

} // namespace LitePlatform
