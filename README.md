# lite
![screenshot](https://user-images.githubusercontent.com/3920290/81471642-6c165880-91ea-11ea-8cd1-fae7ae8f0bc4.png)

A lightweight text editor written in Lua

* **[Get lite](https://github.com/rxi/lite/releases/latest)** — Download
  for Windows and Linux
* **[Get started](doc/usage.md)** — A quick overview on how to get started
* **[Get plugins](https://github.com/rxi/lite-plugins)** — Add additional
  functionality
* **[Get color themes](https://github.com/rxi/lite-colors)** — Add additional colors
  themes

## Overview
lite is a lightweight text editor written mostly in Lua — it aims to provide
something practical, pretty, *small* and fast, implemented as simply as
possible; easy to modify and extend, or to use without doing either.

The editor core is backed by a portable C++17 binding built on
[Dear ImGui](https://github.com/ocornut/imgui): it renders through an
`ImDrawList` and never touches a window system or a graphics API directly, so
it can be embedded in any Dear ImGui application. See
[doc/bindings.md](doc/bindings.md) for the embedding API.

## Customization
Additional functionality can be added through plugins which are available from
the [plugins repository](https://github.com/rxi/lite-plugins); additional color
themes can be found in the [colors repository](https://github.com/rxi/lite-colors).
The editor can be customized by making changes to the
[user module](data/user/init.lua).

## Building
On Windows, [MinGW-w64](https://www.mingw-w64.org/) (`g++`, `gcc`, `ar` and
`windres` in `PATH`) is required. The build scripts live in `build\mingw32`:

* `build\mingw32\build_static.bat` — builds
  `build\mingw32\dist\liblite_binding.a`, a static library containing Lua,
  Dear ImGui and the portable lite binding, plus the public header
  `build\mingw32\dist\include\Lite.h`.
* `build\mingw32\build_exe.bat` — builds the editor itself: Dear ImGui's SDL2 +
  OpenGL3 example with lite added as an ImGui window. The result is
  `build\mingw32\dist\lite.exe`, together with `SDL2.dll` and the staged `data`
  directory, so it can be run directly.

Both scripts are examples; any C++17 build system can produce the same two
targets. Changing the Lua portion of the code does not require a rebuild: copy
`src/lua` to `build\mingw32\dist\data` (which `build_exe.bat` does
automatically).

## Contributing
Any additional functionality that can be added through a plugin should be done
so as a plugin, after which a pull request to the
[plugins repository](https://github.com/rxi/lite-plugins) can be made. In hopes
of remaining lightweight, pull requests adding additional functionality to the
core will likely not be merged. Bug reports and bug fixes are welcome.

## License
This project is free software; you can redistribute it and/or modify it under
the terms of the MIT license. See [LICENSE](LICENSE) for details.
