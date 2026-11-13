# Embedding lite

The native part of lite is split into two pieces:

* `src/bindings` — the portable binding. It owns the Lua state, registers the
  `system` and `renderer` Lua libraries, and turns the editor's draw commands
  into Dear ImGui draw list commands. It has no dependency on a window system
  or on a graphics API.
* `src/host/main.cpp` — the example host. It is Dear ImGui's official SDL2 +
  OpenGL3 example with the editor added as a full size ImGui window. It
  supplies the window, the GL context, the ImGui backends and the input
  translation.

## Requirements

* C++17.
* Dear ImGui 1.92 or newer, with a platform backend and a renderer backend.
  The renderer backend should support `ImGuiBackendFlags_RendererHasTextures`
  so fonts loaded at runtime can be added to the atlas. Hosts on a legacy
  backend must load every font before the first frame; `Lite::PrepareFrame()`
  then handles the atlas outside of a frame.
* The static library produced by `build\mingw32\build_static.bat` already
  contains Lua 5.2 and the Dear ImGui core. Link the application against it and
  compile only the backends (and `imgui_demo.cpp`, if the demo window is used);
  do not compile `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp` or
  `imgui_widgets.cpp` a second time.

## Frame loop

```cpp
Lite lite;
LiteHost host;
host.Scale = displayScale;              // e.g. SDL_GetDisplayDPI() / 96
host.SetWindowTitle = [](const char *title) { /* ... */ };
host.WindowHasFocus = [] { return true; };
host.Confirm = [](const char *title, const char *message) { return true; };
if (!lite.Initialize(argc, argv, host)) { /* show lite.GetErrorMessage() */ }

while (!done) {
  // Queue window events (see below) with lite.PushEvent().

  lite.PrepareFrame();                  // before ImGui::NewFrame()
  ImGui::NewFrame();

  ImGui::Begin("editor", nullptr, flags);   // no padding, no decoration
  lite.BeginFrame(
    ImGui::GetWindowDrawList(),
    ImGui::GetCursorScreenPos(),
    ImGui::GetContentRegionAvail());
  lite.Update();                        // runs one Lua frame and draws
  lite.EndFrame();
  ImGui::End();

  if (lite.IsExitRequested()) done = true;

  ImGui::Render();
  /* render draw data with the backend */
}
lite.Shutdown();
```

`BeginFrame()` and `EndFrame()` must run between `ImGui::NewFrame()` and
`ImGui::Render()`. The origin and size passed to `BeginFrame()` define the
editor's coordinate space: mouse events must be translated to that space.

`lite.Update()` always asks the Lua core to redraw. Dear ImGui has no
persistent surface, so the editor re-emits its draw commands on every frame.

## Events

`LiteEvent` maps one to one to the values returned by `system.poll_event()`:

| Event                    | Lua values                                      |
| ------------------------ | ----------------------------------------------- |
| `MakeQuit()`             | `"quit"`                                        |
| `MakeResized(w, h)`      | `"resized", w, h`                               |
| `MakeExposed()`          | `"exposed"`                                     |
| `MakeKeyPressed(key)`    | `"keypressed", key`                             |
| `MakeKeyReleased(key)`   | `"keyreleased", key`                            |
| `MakeTextInput(text)`    | `"textinput", text`                             |
| `MakeMousePressed(...)`  | `"mousepressed", button, x, y, clicks`          |
| `MakeMouseReleased(...)` | `"mousereleased", button, x, y`                 |
| `MakeMouseMoved(...)`    | `"mousemoved", x, y, dx, dy`                    |
| `MakeMouseWheel(delta)`  | `"mousewheel", delta`                           |
| `MakeFileDropped(...)`   | `"filedropped", filename, x, y`                 |

Key names use the SDL spelling, lower cased: `"a"`, `"return"`, `"pageup"`,
`"left ctrl"`, `"f5"`. Mouse buttons are `"left"`, `"middle"` and `"right"`.

The example host forwards mouse events only while its editor window is hovered
(or a drag started there) and keyboard events only while it is focused, leaving
the rest of the ImGui application its own input.

## Host services

`LiteHost` delegates everything that needs the host:

* `SetWindowTitle` / `SetWindowMode` — the Lua core changes the window title
  when the active view changes, and `alt+return` toggles fullscreen.
* `WindowHasFocus` — used for caret blinking and idle handling. A `false`
  result from an absent callback falls back to `Lite::SetFocused()`.
* `Confirm` — yes/no dialog used when quitting with unsaved documents.

The clipboard is delegated to Dear ImGui (`ImGui::GetClipboardText()` /
`ImGui::SetClipboardText()`); the ImGui platform backend must provide those
functions, as the SDL2 and GLFW backends do.

Calling `os.exit()` from Lua (which is what `core:quit` does) is captured by
the binding: the host loop sees `IsExitRequested()` and can shut down cleanly.

## Fonts and file names

Fonts are added to the host's `ImFontAtlas` and text is drawn with
`ImDrawList::AddText()`, using `PixelSnapH` so the measured and rendered
advances agree.

Lua 5.2 opens files through the C runtime, which on Windows interprets narrow
paths in the active ANSI code page. The binding therefore exchanges file names
with Lua in that native encoding, like the original lite bindings did; Dear
ImGui's font loader needs UTF-8, so the binding converts at that boundary. On
other platforms names are UTF-8.

## Builds

* `build\mingw32\build_static.bat` → `build\mingw32\dist\liblite_binding.a`,
  `build\mingw32\dist\include\Lite.h`.
* `build\mingw32\build_exe.bat` → `build\mingw32\dist\lite.exe`,
  `build\mingw32\dist\SDL2.dll`, `build\mingw32\dist\data\`.
