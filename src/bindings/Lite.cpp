#include "Lite.h"

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif

#include "LiteApi.h"
#include "LiteLua.h"
#include "LitePlatform.h"
#include "LiteRenderer.h"


/* SDL2 hands the command line over as UTF-8, while Lua's file functions use
** the native narrow encoding. Convert the arguments so that system.* and the
** Lua core agree on file names. */
static std::string ToNativePath(const char *text) {
#if defined(_WIN32)
  if (!text) {
    return std::string();
  }

  int wideLength = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
  if (wideLength <= 0) {
    return std::string(text);
  }
  std::wstring wide((size_t) wideLength, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text, -1, &wide[0], wideLength);

  int nativeLength = WideCharToMultiByte(
    CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (nativeLength <= 0) {
    return std::string(text);
  }
  std::string native((size_t) nativeLength, '\0');
  WideCharToMultiByte(
    CP_ACP, 0, wide.c_str(), -1, &native[0], nativeLength, nullptr, nullptr);
  native.resize((size_t) nativeLength - 1);
  return native;
#else
  return std::string(text ? text : "");
#endif
}

static const char *const LITE_INIT_SCRIPT = R"LUA(
local core
local function handler(err)
  return debug.traceback(tostring(err), 2)
end
local ok, err = xpcall(function()
  SCALE = tonumber(os.getenv("LITE_SCALE")) or SCALE
  PATHSEP = package.config:sub(1, 1)
  EXEDIR = EXEFILE:match("^(.+)[/\\].*$")
  package.path = EXEDIR .. '/data/?.lua;' .. package.path
  package.path = EXEDIR .. '/data/?/init.lua;' .. package.path
  core = require('core')
  core.init()
end, handler)
if not ok then
  print('Error: ' .. tostring(err))
  if core and core.on_error then pcall(core.on_error, err) end
  return false, err
end
return true, core
)LUA";


static Lite *GetLiteFromRegistry(lua_State *L) {
  lua_getfield(L, LUA_REGISTRYINDEX, LITE_INSTANCE_KEY);
  Lite *lite = (Lite *) lua_touserdata(L, -1);
  lua_pop(L, 1);
  return lite;
}


/* os.exit() is what core.quit() uses to leave the editor. In an embedded
** editor the host owns the process, so remember the request instead and let
** the main loop shut down cleanly. */
int LiteOsExit(lua_State *L) {
  int status = EXIT_SUCCESS;
  if (lua_isboolean(L, 1)) {
    status = lua_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE;
  } else {
    status = (int) luaL_optinteger(L, 1, EXIT_SUCCESS);
  }

  Lite *lite = GetLiteFromRegistry(L);
  if (lite) {
    lite->m_exitRequested = true;
    lite->m_exitCode = status;
  }
  return 0;
}


LiteEvent LiteEvent::MakeQuit() {
  LiteEvent event;
  event.Type = LiteEventType::Quit;
  return event;
}


LiteEvent LiteEvent::MakeResized(int width, int height) {
  LiteEvent event;
  event.Type = LiteEventType::Resized;
  event.A = width;
  event.B = height;
  return event;
}


LiteEvent LiteEvent::MakeExposed() {
  LiteEvent event;
  event.Type = LiteEventType::Exposed;
  return event;
}


LiteEvent LiteEvent::MakeKeyPressed(const char *key) {
  LiteEvent event;
  event.Type = LiteEventType::KeyPressed;
  event.Text = key ? key : "";
  return event;
}


LiteEvent LiteEvent::MakeKeyReleased(const char *key) {
  LiteEvent event;
  event.Type = LiteEventType::KeyReleased;
  event.Text = key ? key : "";
  return event;
}


LiteEvent LiteEvent::MakeTextInput(const char *text) {
  LiteEvent event;
  event.Type = LiteEventType::TextInput;
  event.Text = text ? text : "";
  return event;
}


LiteEvent LiteEvent::MakeMousePressed(
  const char *button,
  int x,
  int y,
  int clicks
) {
  LiteEvent event;
  event.Type = LiteEventType::MousePressed;
  event.Text = button ? button : "?";
  event.A = x;
  event.B = y;
  event.C = clicks;
  return event;
}


LiteEvent LiteEvent::MakeMouseReleased(const char *button, int x, int y) {
  LiteEvent event;
  event.Type = LiteEventType::MouseReleased;
  event.Text = button ? button : "?";
  event.A = x;
  event.B = y;
  return event;
}


LiteEvent LiteEvent::MakeMouseMoved(int x, int y, int dx, int dy) {
  LiteEvent event;
  event.Type = LiteEventType::MouseMoved;
  event.A = x;
  event.B = y;
  event.C = dx;
  event.D = dy;
  return event;
}


LiteEvent LiteEvent::MakeMouseWheel(int delta) {
  LiteEvent event;
  event.Type = LiteEventType::MouseWheel;
  event.A = delta;
  return event;
}


LiteEvent LiteEvent::MakeFileDropped(const char *filename, int x, int y) {
  LiteEvent event;
  event.Type = LiteEventType::FileDropped;
  event.Text = filename ? filename : "";
  event.A = x;
  event.B = y;
  return event;
}


Lite::Lite() {
  m_renderer = new LiteRenderer();
}


Lite::~Lite() {
  Shutdown();
  delete m_renderer;
  m_renderer = nullptr;
}


bool Lite::Initialize(int argc, char **argv, const LiteHost &host) {
  if (m_lua) {
    return true;
  }
  m_errorMessage.clear();
  m_windowTitle.clear();
  if (!ImGui::GetCurrentContext()) {
    m_errorMessage = "Lite::Initialize() requires an active ImGui context";
    return false;
  }

  m_host = host;
  if (m_host.Scale <= 0.0f) {
    m_host.Scale = 1.0f;
  }
  m_platformName = LitePlatform::GetName();
  m_executablePath = LitePlatform::GetExecutablePath();
  if (m_executablePath.empty() && argc > 0 && argv && argv[0]) {
    m_executablePath = argv[0];
  }

  m_renderer->Initialize(ImGui::GetIO().Fonts);

  if (!InitializeLua(argc, argv)) {
    Shutdown();
    return false;
  }
  if (!RunInitScript()) {
    fprintf(stderr, "%s\n", m_errorMessage.c_str());
    Shutdown();
    return false;
  }

  m_renderer->Prepare();
  return true;
}


bool Lite::InitializeLua(int argc, char **argv) {
  lua_State *L = luaL_newstate();
  if (!L) {
    m_errorMessage = "failed to create Lua state";
    return false;
  }
  m_lua = L;
  luaL_openlibs(L);

  lua_pushlightuserdata(L, this);
  lua_setfield(L, LUA_REGISTRYINDEX, LITE_INSTANCE_KEY);

  luaL_requiref(L, "system", LiteOpenSystem, 1);
  lua_pop(L, 1);
  luaL_requiref(L, "renderer", LiteOpenRenderer, 1);
  lua_pop(L, 1);

  lua_newtable(L);
  for (int i = 0; i < argc; i++) {
    std::string argument = ToNativePath(argv[i]);
    lua_pushlstring(L, argument.c_str(), argument.size());
    lua_rawseti(L, -2, i + 1);
  }
  lua_setglobal(L, "ARGS");

  lua_pushliteral(L, "1.11");
  lua_setglobal(L, "VERSION");

  lua_pushlstring(L, m_platformName.c_str(), m_platformName.size());
  lua_setglobal(L, "PLATFORM");

  lua_pushnumber(L, m_host.Scale);
  lua_setglobal(L, "SCALE");

  lua_pushlstring(L, m_executablePath.c_str(), m_executablePath.size());
  lua_setglobal(L, "EXEFILE");

  OverrideOsExit();
  return true;
}


void Lite::OverrideOsExit() {
  lua_State *L = m_lua;
  lua_getglobal(L, "os");
  if (lua_istable(L, -1)) {
    lua_pushcfunction(L, LiteOsExit);
    lua_setfield(L, -2, "exit");
  }
  lua_pop(L, 1);
}


bool Lite::RunInitScript() {
  lua_State *L = m_lua;

  if (luaL_loadstring(L, LITE_INIT_SCRIPT) != LUA_OK) {
    const char *message = lua_tostring(L, -1);
    m_errorMessage = message ? message : "failed to load the init script";
    lua_pop(L, 1);
    return false;
  }

  if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
    const char *message = lua_tostring(L, -1);
    m_errorMessage = message ? message : "failed to run the init script";
    luaL_traceback(L, L, m_errorMessage.c_str(), 1);
    const char *traceback = lua_tostring(L, -1);
    if (traceback) {
      m_errorMessage = traceback;
    }
    lua_pop(L, 2);
    return false;
  }

  int resultCount = lua_gettop(L);
  bool ok = resultCount >= 1 && lua_toboolean(L, 1) != 0;
  if (ok && resultCount >= 2 && lua_istable(L, 2)) {
    /* `core` is a local in the init script; keep the table where the C side
    ** can reach it. The Lua strict module forbids new globals, and a global
    ** would also be visible to user code that did not ask for it. */
    lua_pushvalue(L, 2);
    lua_setfield(L, LUA_REGISTRYINDEX, LITE_CORE_KEY);
  }
  if (!ok && resultCount >= 2) {
    const char *message = lua_tostring(L, 2);
    if (message) {
      m_errorMessage = message;
    }
  }
  lua_settop(L, 0);
  return ok;
}


bool Lite::CallTick() {
  lua_State *L = m_lua;

  lua_getfield(L, LUA_REGISTRYINDEX, LITE_CORE_KEY);
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    ReportError("core is not available");
    return false;
  }

  lua_getfield(L, -1, "tick");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    ReportError("core.tick is not available");
    return false;
  }

  /* Always redraw: Dear ImGui rebuilds the frame from scratch every time, so
  ** there is no persistent surface that could keep the previous one. */
  lua_pushboolean(L, 1);
  if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
    const char *message = lua_tostring(L, -1);
    ReportError(message ? message : "unknown error");
    lua_pop(L, 2);
    return false;
  }

  bool redrawn = lua_toboolean(L, -1) != 0;
  lua_pop(L, 2);
  return redrawn;
}


void Lite::ReportError(const char *message) {
  lua_State *L = m_lua;
  const char *text = message ? message : "unknown error";

  luaL_traceback(L, L, text, 1);
  const char *traceback = lua_tostring(L, -1);
  m_errorMessage = traceback ? traceback : text;
  lua_pop(L, 1);

  fprintf(stderr, "%s\n", m_errorMessage.c_str());

  /* Give the Lua core a chance to save unsaved documents and write
  ** error.txt. */
  lua_getfield(L, LUA_REGISTRYINDEX, LITE_CORE_KEY);
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "on_error");
    if (lua_isfunction(L, -1)) {
      lua_pushstring(L, text);
      if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        lua_pop(L, 1);
      }
    } else {
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);
}


void Lite::Shutdown() {
  if (m_lua) {
    lua_close(m_lua);
    m_lua = nullptr;
  }
  if (m_renderer) {
    m_renderer->Terminate();
  }
  m_events.clear();
  m_exitRequested = false;
  m_exitCode = 0;
}


void Lite::PushEvent(const LiteEvent &event) {
  m_events.push_back(event);
}


bool Lite::PollEvent(LiteEvent &event) {
  if (m_events.empty()) {
    return false;
  }
  event = m_events.front();
  m_events.pop_front();
  return true;
}


void Lite::PrepareFrame() {
  if (m_renderer) {
    m_renderer->Prepare();
  }
}


void Lite::BeginFrame(
  ImDrawList *drawList,
  const ImVec2 &origin,
  const ImVec2 &size
) {
  if (!m_renderer) {
    return;
  }
  m_renderer->BeginFrame(drawList, origin, size);
}


bool Lite::Update() {
  if (!m_lua || m_exitRequested) {
    return false;
  }
  return CallTick();
}


void Lite::EndFrame() {
  if (m_renderer) {
    m_renderer->EndFrame();
  }
}


void Lite::SetWindowTitle(const char *title) {
  m_windowTitle = title ? title : "";
  if (m_host.SetWindowTitle) {
    m_host.SetWindowTitle(m_windowTitle.c_str());
  }
}


void Lite::SetWindowMode(LiteWindowMode mode) {
  if (m_host.SetWindowMode) {
    m_host.SetWindowMode(mode);
  }
}


bool Lite::WindowHasFocus() const {
  if (m_host.WindowHasFocus) {
    return m_host.WindowHasFocus();
  }
  return m_focused;
}


bool Lite::Confirm(const char *title, const char *message) {
  if (m_host.Confirm) {
    return m_host.Confirm(title, message);
  }
  return true;
}
