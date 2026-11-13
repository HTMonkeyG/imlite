#ifndef __LITE_BINDING_HPP__
#define __LITE_BINDING_HPP__

#include <deque>
#include <functional>
#include <string>

#include "imgui.h"

struct lua_State;

/* The editor's color format. Mirrors the { r, g, b, a } table used on the
** Lua side, with one byte per component. */
struct LiteColor {
  unsigned char r, g, b, a;

  LiteColor(
    unsigned char r = 255,
    unsigned char g = 255,
    unsigned char b = 255,
    unsigned char a = 255
  ) : r(r), g(g), b(b), a(a) {}
};

enum LiteWindowMode {
  LITE_WINDOW_NORMAL     = 0,
  LITE_WINDOW_MAXIMIZED  = 1,
  LITE_WINDOW_FULLSCREEN = 2
};

/* Everything the editor cannot do by itself is delegated to the host through
** this structure. Keeping the window system and the graphics API on the host
** side is what makes the binding backend independent: the editor itself only
** talks to an ImDrawList. */
struct LiteHost {
  /* Display scale (DPI) used to size the editor, e.g. 1.0 or 1.5. */
  float Scale = 1.0f;

  /* Called when the editor changes its window title. */
  std::function<void(const char *title)> SetWindowTitle;

  /* Called when the editor requests a window mode change. */
  std::function<void(LiteWindowMode mode)> SetWindowMode;

  /* Returns whether the host window currently has keyboard focus. */
  std::function<bool()> WindowHasFocus;

  /* Shows a yes/no dialog and returns the user's answer. */
  std::function<bool(const char *title, const char *message)> Confirm;
};

enum class LiteEventType {
  Quit,
  Resized,
  Exposed,
  KeyPressed,
  KeyReleased,
  TextInput,
  MousePressed,
  MouseReleased,
  MouseMoved,
  MouseWheel,
  FileDropped
};

/* A window event translated to the vocabulary the Lua core expects. The
** numeric arguments map one to one to the values returned by
** system.poll_event(). */
struct LiteEvent {
  LiteEventType Type = LiteEventType::Quit;
  std::string Text;
  int A = 0;
  int B = 0;
  int C = 0;
  int D = 0;

  static LiteEvent MakeQuit();
  static LiteEvent MakeResized(int width, int height);
  static LiteEvent MakeExposed();
  static LiteEvent MakeKeyPressed(const char *key);
  static LiteEvent MakeKeyReleased(const char *key);
  static LiteEvent MakeTextInput(const char *text);
  static LiteEvent MakeMousePressed(const char *button, int x, int y, int clicks);
  static LiteEvent MakeMouseReleased(const char *button, int x, int y);
  static LiteEvent MakeMouseMoved(int x, int y, int dx, int dy);
  static LiteEvent MakeMouseWheel(int delta);
  static LiteEvent MakeFileDropped(const char *filename, int x, int y);
};

class LiteRenderer;

/* The editor instance. One Lite may live in any application that has a Dear
** ImGui context; it owns a Lua state and a renderer, and draws itself into a
** caller supplied ImDrawList.
**
** Usage per frame:
**   lite.PrepareFrame();            // before ImGui::NewFrame()
**   ImGui::NewFrame();
**   ... Begin a window ...
**   lite.BeginFrame(draw_list, origin, size);
**   lite.Update();
**   lite.EndFrame();
**   ... End the window ...
**   ImGui::Render(); */
class Lite {
public:
  Lite();
  ~Lite();
  Lite(const Lite &) = delete;
  Lite &operator=(const Lite &) = delete;

  /* Creates the Lua state, loads the Lua core and its bindings. Requires an
  ** active ImGui context. */
  bool Initialize(int argc, char **argv, const LiteHost &host);

  /* Closes the Lua state and releases the renderer. */
  void Shutdown();

  bool IsInitialized() const { return m_lua != nullptr; }

  /* Queues a window event for system.poll_event(). Events are consumed by the
  ** next call to Update(). */
  void PushEvent(const LiteEvent &event);

  /* Pops one queued event into `event`. Returns false when the queue is
  ** empty. Used by the Lua binding. */
  bool PollEvent(LiteEvent &event);

  /* Tells the editor whether the host window is focused. Kept for hosts that
  ** do not provide LiteHost::WindowHasFocus. */
  void SetFocused(bool focused) { m_focused = focused; }

  /* Runs pending font work (creation/removal). Must be called before
  ** ImGui::NewFrame(), and is required for hosts whose renderer backend does
  ** not support dynamic textures. Safe to call every frame. */
  void PrepareFrame();

  /* Sets the draw list and the editor area for this frame. `origin` is the
  ** screen position of the editor's top left corner, `size` its size. */
  void BeginFrame(ImDrawList *drawList, const ImVec2 &origin, const ImVec2 &size);

  /* Advances the editor by one frame. Returns true when Lua drew something. */
  bool Update();

  /* Closes the editor area opened by BeginFrame(). */
  void EndFrame();

  /* True once the Lua code called os.exit(), e.g. after the user confirmed
  ** core:quit. The host should stop its main loop and call Shutdown(). */
  bool IsExitRequested() const { return m_exitRequested; }
  int GetExitCode() const { return m_exitCode; }

  lua_State *GetLuaState() const { return m_lua; }
  LiteRenderer *GetRenderer() const { return m_renderer; }
  const LiteHost &GetHost() const { return m_host; }

  const char *GetWindowTitle() const { return m_windowTitle.c_str(); }
  const char *GetExecutablePath() const { return m_executablePath.c_str(); }
  const char *GetPlatformName() const { return m_platformName.c_str(); }
  const char *GetErrorMessage() const { return m_errorMessage.c_str(); }

  /* Services used by the Lua bindings. */
  void SetWindowTitle(const char *title);
  void SetWindowMode(LiteWindowMode mode);
  bool WindowHasFocus() const;
  bool Confirm(const char *title, const char *message);

private:
  bool InitializeLua(int argc, char **argv);
  bool RunInitScript();
  bool CallTick();
  void OverrideOsExit();
  void ReportError(const char *message);

  lua_State *m_lua = nullptr;
  LiteRenderer *m_renderer = nullptr;
  LiteHost m_host;
  std::deque<LiteEvent> m_events;
  std::string m_windowTitle;
  std::string m_executablePath;
  std::string m_platformName;
  std::string m_errorMessage;
  bool m_focused = true;
  bool m_exitRequested = false;
  int m_exitCode = 0;

  friend int LiteOsExit(lua_State *L);
};

#endif
