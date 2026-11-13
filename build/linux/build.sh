#!/bin/bash
# Builds the lite editor on Linux with Dear ImGui's SDL2 + OpenGL3 backends.
# Requires g++, pkg-config, SDL2 development files and OpenGL.
# The Windows equivalent is build\mingw32\build_exe.bat.

cd "$(dirname "$0")/../.." || exit 1

OUT="build/dist"
OBJ="build/obj/linux"
mkdir -p "$OUT" "$OBJ"

CXXFLAGS="-std=c++17 -O2 -Wall -fno-strict-aliasing -Ilibraries/imgui -Ilibraries/imgui/backends -Ilibraries/lua -Isrc/bindings"
CFLAGS="-O2 -fno-strict-aliasing -DLUA_USE_POSIX -Ilibraries/lua"
SDL_CFLAGS="$(pkg-config --cflags sdl2)"
SDL_LIBS="$(pkg-config --libs sdl2)"

echo "building lua..."
for f in libraries/lua/*.c; do
  case "$(basename "$f")" in
    lua.c|ltests.c) continue ;;
  esac
  gcc $CFLAGS -c "$f" -o "$OBJ/$(basename "${f%.c}").o" || exit 1
done

echo "building dear imgui..."
for f in imgui imgui_draw imgui_tables imgui_widgets; do
  g++ $CXXFLAGS -c "libraries/imgui/$f.cpp" -o "$OBJ/$f.o" || exit 1
done

echo "building lite bindings..."
for f in src/bindings/*.cpp; do
  g++ $CXXFLAGS -c "$f" -o "$OBJ/$(basename "${f%.cpp}").o" || exit 1
done

echo "building host..."
g++ $CXXFLAGS $SDL_CFLAGS -c src/host/main.cpp -o "$OBJ/main.o" || exit 1
g++ $CXXFLAGS -c libraries/imgui/imgui_demo.cpp -o "$OBJ/imgui_demo.o" || exit 1
g++ $CXXFLAGS $SDL_CFLAGS -c libraries/imgui/backends/imgui_impl_sdl2.cpp -o "$OBJ/imgui_impl_sdl2.o" || exit 1
g++ $CXXFLAGS -c libraries/imgui/backends/imgui_impl_opengl3.cpp -o "$OBJ/imgui_impl_opengl3.o" || exit 1

echo "linking..."
g++ -o "$OUT/lite" "$OBJ"/*.o $SDL_LIBS -lGL -ldl -lpthread || exit 1

echo "staging data..."
rm -rf "$OUT/data"
cp -r src/lua "$OUT/data"

echo "done: $OUT/lite"
