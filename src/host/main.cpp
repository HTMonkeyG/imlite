// lite host: Dear ImGui's SDL2 + OpenGL3 example, with the lite editor
// embedded as a full size ImGui window. The editor itself only talks to an
// ImDrawList (see src/bindings/Lite.h); this file supplies the window, the
// graphics context, the input translation and the host services.
//
// Based on libraries/imgui/examples/example_sdl2_opengl3/main.cpp.

#include "Lite.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#include <cctype>
#include <cstdio>
#include <string>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>        // SetProcessDPIAware()
#endif

static SDL_Window *g_window = nullptr;

// Input routing. ImGui owns the windows; lite receives the events only while
// its window is hovered (mouse) or focused (keyboard). The flags are read back
// after rendering, which is one frame behind the SDL events: that is the usual
// immediate-mode trade-off and is not noticeable in practice.
static bool g_liteHovered = true;
static bool g_liteFocused = true;
static bool g_liteCaptured = false;
static bool g_firstFrame = true;

static std::string g_windowTitle;
static bool g_showDemo = false;


static const char *ButtonName(int button) {
  switch (button) {
    case 1: return "left";
    case 2: return "middle";
    case 3: return "right";
    default: return "?";
  }
}


static std::string KeyName(SDL_Keycode key) {
  char buffer[64];
  SDL_snprintf(buffer, sizeof(buffer), "%s", SDL_GetKeyName(key));
  for (char *cursor = buffer; *cursor; cursor++) {
    *cursor = (char) std::tolower((unsigned char) *cursor);
  }
  return std::string(buffer);
}


static void PushKey(Lite &lite, bool pressed, SDL_Keycode key) {
  std::string name = KeyName(key);
  lite.PushEvent(pressed
    ? LiteEvent::MakeKeyPressed(name.c_str())
    : LiteEvent::MakeKeyReleased(name.c_str()));
}


static void TranslateEvent(const SDL_Event &event, Lite &lite) {
  switch (event.type) {
    case SDL_QUIT:
      lite.PushEvent(LiteEvent::MakeQuit());
      break;

    case SDL_WINDOWEVENT:
      if (event.window.event == SDL_WINDOWEVENT_RESIZED
      || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        lite.PushEvent(LiteEvent::MakeResized(event.window.data1, event.window.data2));
      } else if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
        lite.PushEvent(LiteEvent::MakeExposed());
      } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
        // Alt-tabbing can leave key-down events queued for the tab key.
        SDL_FlushEvent(SDL_KEYDOWN);
        g_liteCaptured = false;
      }
      break;

    case SDL_DROPFILE: {
      int mouseX = 0, mouseY = 0, windowX = 0, windowY = 0;
      SDL_GetGlobalMouseState(&mouseX, &mouseY);
      SDL_GetWindowPosition(g_window, &windowX, &windowY);
      lite.PushEvent(LiteEvent::MakeFileDropped(
        event.drop.file, mouseX - windowX, mouseY - windowY));
      SDL_free(event.drop.file);
      break;
    }

    case SDL_KEYDOWN:
      if (g_liteFocused) {
        PushKey(lite, true, event.key.keysym.sym);
      }
      break;

    case SDL_KEYUP:
      if (g_liteFocused) {
        PushKey(lite, false, event.key.keysym.sym);
      }
      break;

    case SDL_TEXTINPUT:
      if (g_liteFocused) {
        lite.PushEvent(LiteEvent::MakeTextInput(event.text.text));
      }
      break;

    case SDL_MOUSEBUTTONDOWN:
      if (g_liteHovered) {
        lite.PushEvent(LiteEvent::MakeMousePressed(
          ButtonName(event.button.button),
          event.button.x,
          event.button.y,
          event.button.clicks));
        if (event.button.button == SDL_BUTTON_LEFT) {
          g_liteCaptured = true;
        }
      }
      break;

    case SDL_MOUSEBUTTONUP:
      if (g_liteHovered || g_liteCaptured) {
        lite.PushEvent(LiteEvent::MakeMouseReleased(
          ButtonName(event.button.button),
          event.button.x,
          event.button.y));
      }
      if (event.button.button == SDL_BUTTON_LEFT) {
        g_liteCaptured = false;
      }
      break;

    case SDL_MOUSEMOTION:
      if (g_liteHovered || g_liteCaptured) {
        lite.PushEvent(LiteEvent::MakeMouseMoved(
          event.motion.x,
          event.motion.y,
          event.motion.xrel,
          event.motion.yrel));
      }
      break;

    case SDL_MOUSEWHEEL:
      if (g_liteHovered) {
        lite.PushEvent(LiteEvent::MakeMouseWheel(event.wheel.y));
      }
      break;

    default:
      break;
  }
}


static bool ConfirmDialog(const char *title, const char *message) {
  SDL_MessageBoxButtonData buttons[] = {
    { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
    { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No" },
  };
  SDL_MessageBoxData data = {};
  data.flags = SDL_MESSAGEBOX_WARNING;
  data.window = g_window;
  data.title = title;
  data.message = message;
  data.numbuttons = 2;
  data.buttons = buttons;

  int buttonId = 0;
  if (SDL_ShowMessageBox(&data, &buttonId) < 0) {
    return true;
  }
  return buttonId == 1;
}


static void DrawLiteWindow(Lite &lite) {
  ImGuiIO &io = ImGui::GetIO();

  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_NoResize
    | ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoCollapse
    | ImGuiWindowFlags_NoScrollbar
    | ImGuiWindowFlags_NoScrollWithMouse
    | ImGuiWindowFlags_NoSavedSettings
    | ImGuiWindowFlags_NoBackground
    | ImGuiWindowFlags_NoBringToFrontOnFocus;

  if (g_firstFrame) {
    ImGui::SetNextWindowFocus();
    g_firstFrame = false;
  }

  ImGui::Begin("lite", nullptr, flags);
  {
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    lite.BeginFrame(drawList, origin, size);
    lite.Update();
    lite.EndFrame();

    g_liteHovered = ImGui::IsWindowHovered(
      ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    g_liteFocused = ImGui::IsWindowFocused();
  }
  ImGui::End();

  ImGui::PopStyleVar(3);
}


static void DrawHostWindow() {
  ImGuiIO &io = ImGui::GetIO();

  ImGui::SetNextWindowPos(
    ImVec2(io.DisplaySize.x - 12.0f, 12.0f), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(260.0f, 0.0f), ImGuiCond_FirstUseEver);

  ImGui::Begin("Host");
  ImGui::Text("Dear ImGui %s", IMGUI_VERSION);
  ImGui::Checkbox("Show demo window", &g_showDemo);
  if (ImGui::Button("Focus editor")) {
    ImGui::SetWindowFocus("lite");
  }
  ImGui::Text("%.1f FPS", io.Framerate);
  ImGui::End();
}


static void UpdateWindowTitle(Lite &lite) {
  const char *title = lite.GetWindowTitle();
  if (title && g_windowTitle != title) {
    g_windowTitle = title;
    SDL_SetWindowTitle(g_window, title);
  }
}


int main(int argc, char **argv) {
#ifdef _WIN32
  ::SetProcessDPIAware();
#endif

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
    printf("Error: %s\n", SDL_GetError());
    return -1;
  }

#if defined(IMGUI_IMPL_OPENGL_ES2)
  const char *glslVersion = "#version 100";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
  const char *glslVersion = "#version 300 es";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
  const char *glslVersion = "#version 150";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
  const char *glslVersion = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

#ifdef SDL_HINT_IME_SHOW_UI
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 5)
  SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#endif
#ifdef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
  SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  float mainScale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);

  SDL_DisplayMode displayMode = {};
  int windowWidth = 1280;
  int windowHeight = 800;
  if (SDL_GetCurrentDisplayMode(0, &displayMode) == 0) {
    windowWidth = (int) (displayMode.w * 0.8f);
    windowHeight = (int) (displayMode.h * 0.8f);
    if (windowWidth < 800) windowWidth = 800;
    if (windowHeight < 600) windowHeight = 600;
  }

  SDL_WindowFlags windowFlags = (SDL_WindowFlags)(
    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  g_window = SDL_CreateWindow(
    "lite",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    windowWidth,
    windowHeight,
    windowFlags);
  if (!g_window) {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return -1;
  }

  SDL_GLContext glContext = SDL_GL_CreateContext(g_window);
  if (!glContext) {
    printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
    return -1;
  }
  SDL_GL_MakeCurrent(g_window, glContext);
  SDL_GL_SetSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsDark();
  ImGui::GetStyle().ScaleAllSizes(mainScale);

  ImGui_ImplSDL2_InitForOpenGL(g_window, glContext);
  ImGui_ImplOpenGL3_Init(glslVersion);

  // The host UI keeps the default Dear ImGui font; lite adds its own fonts.
  io.Fonts->AddFontDefault();

  LiteHost host;
  host.Scale = mainScale;
  host.SetWindowTitle = [](const char *title) {
    SDL_SetWindowTitle(g_window, title);
  };
  host.SetWindowMode = [](LiteWindowMode mode) {
    switch (mode) {
      case LITE_WINDOW_FULLSCREEN:
        SDL_SetWindowFullscreen(g_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        break;
      case LITE_WINDOW_MAXIMIZED:
        SDL_MaximizeWindow(g_window);
        break;
      default:
        SDL_SetWindowFullscreen(g_window, 0);
        SDL_RestoreWindow(g_window);
        break;
    }
  };
  host.WindowHasFocus = []() {
    return (SDL_GetWindowFlags(g_window) & SDL_WINDOW_INPUT_FOCUS) != 0;
  };
  host.Confirm = [](const char *title, const char *message) {
    return ConfirmDialog(title, message);
  };

  Lite lite;
  if (!lite.Initialize(argc, argv, host)) {
    SDL_ShowSimpleMessageBox(
      SDL_MESSAGEBOX_ERROR, "lite", lite.GetErrorMessage(), g_window);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
    return 1;
  }

  bool done = false;
  Uint64 performanceFrequency = SDL_GetPerformanceFrequency();
  while (!done) {
    Uint64 frameStart = SDL_GetPerformanceCounter();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT) {
        lite.PushEvent(LiteEvent::MakeQuit());
        continue;
      }
      if (event.type == SDL_WINDOWEVENT
      && event.window.event == SDL_WINDOWEVENT_CLOSE
      && event.window.windowID == SDL_GetWindowID(g_window)) {
        lite.PushEvent(LiteEvent::MakeQuit());
        continue;
      }
      TranslateEvent(event, lite);
    }

    if (SDL_GetWindowFlags(g_window) & SDL_WINDOW_MINIMIZED) {
      SDL_Delay(10);
      continue;
    }

    // Fonts may be created/removed by the Lua GC; the atlas must be unlocked
    // for that, so do it before the ImGui frame starts.
    lite.PrepareFrame();

    ImGui_ImplOpenGL3_NewFrame();    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    DrawLiteWindow(lite);
    if (g_showDemo) {
      ImGui::ShowDemoWindow(&g_showDemo);
    }
    DrawHostWindow();

    UpdateWindowTitle(lite);

    ImGui::Render();
    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(g_window, &drawableWidth, &drawableHeight);
    glViewport(0, 0, drawableWidth, drawableHeight);
    glClearColor(0.18f, 0.18f, 0.20f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(g_window);

    if (lite.IsExitRequested()) {
      done = true;
    }

    // Cap the frame rate when the driver ignores vertical sync.
    double elapsed =
      (double) (SDL_GetPerformanceCounter() - frameStart) / (double) performanceFrequency;
    double target = 1.0 / 60.0;
    if (elapsed < target) {
      SDL_Delay((Uint32) ((target - elapsed) * 1000.0));
    }
  }

  int exitCode = lite.GetExitCode();
  lite.Shutdown();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_GL_DeleteContext(glContext);
  SDL_DestroyWindow(g_window);
  SDL_Quit();

  return exitCode;
}
