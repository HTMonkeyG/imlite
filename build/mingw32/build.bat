@echo off

cd %~dp0

rem download this:
rem https://nuwen.net/mingw.html

md dist 2>nul

echo compiling (windows)...

windres ./res.rc -O coff -o ./dist/res.res
gcc ../../src/bindings/*.c^
    ../../src/bindings/api/*.c^
    ../../src/bindings/stb/*.c^
    ../../libraries/lua/*.c^
    -O3 -s -std=gnu11 -fno-strict-aliasing -DLUA_USE_POPEN^
    -I../../src/bindings^
    -I../../libraries/lua^
    -I../../libraries/stb^
    -I./winlib/SDL2-2.0.10/x86_64-w64-mingw32/include^
    -L./winlib/SDL2-2.0.10/x86_64-w64-mingw32/lib^
    -lmingw32 -lm -lSDL2main -lSDL2^
    -mwindows ./dist/res.res^
    -o ./dist/lite.exe

echo done
