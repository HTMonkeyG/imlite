#ifndef __LITE_PLATFORM_HPP__
#define __LITE_PLATFORM_HPP__

#include <string>
#include <vector>

/* Small operating system helpers used by the system Lua library. They are the
** only place in the binding that touches the platform directly; the window
** system and the graphics API stay on the host side. */
namespace LitePlatform {

/* Full path of the running executable, UTF-8 encoded. Empty on failure. */
std::string GetExecutablePath();

/* Human readable platform name, matching what SDL_GetPlatform() used to
** return. */
const char *GetName();

double GetTime();
void Sleep(double seconds);

bool ListDirectory(
  const char *path,
  std::vector<std::string> &entries,
  std::string &error
);

bool GetFileInfo(
  const char *path,
  std::string &type,
  long long &size,
  double &modified,
  std::string &error
);

bool GetAbsolutePath(const char *path, std::string &result);
bool ChangeDirectory(const char *path, std::string &error);

/* Starts a detached process. `command` is a shell command line, matching the
** behaviour of system.exec() in the original bindings. */
void Execute(const char *command);

} // namespace LitePlatform

#endif
