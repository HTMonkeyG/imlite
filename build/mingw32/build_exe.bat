@echo off
rem Builds the lite editor as a standalone executable: Dear ImGui's SDL2 +
rem OpenGL3 example with the lite binding embedded as an ImGui window.
rem
rem Output (relative to this directory):
rem   dist\lite.exe
rem   dist\SDL2.dll
rem   dist\data\        (the Lua core, fonts and plugins)

setlocal EnableDelayedExpansion
cd /d "%~dp0"

rem The build runs from this directory, so every path is relative: this keeps
rem non-ASCII checkout paths away from the linker's library search.
set "ROOT=..\.."
set "OUT=dist"
set "OBJ=obj\exe"
set "SDL=winlib\SDL2-2.0.10\x86_64-w64-mingw32"

call "%~dp0build_static.bat"
if errorlevel 1 exit /b 1

if not exist "%OBJ%" mkdir "%OBJ%"
if exist "%OBJ%\*.o" del /q "%OBJ%\*.o"

set "INCLUDES=-I"%ROOT%\libraries\imgui" -I"%ROOT%\libraries\imgui\backends" -I"%ROOT%\libraries\lua" -I"%ROOT%\src\bindings" -I"%SDL%\include\SDL2""
set "CXXFLAGS=-std=c++17 -O2 -Wall -fno-strict-aliasing %INCLUDES%"

echo [exe] compiling host...
g++ %CXXFLAGS% -c "%ROOT%\src\host\main.cpp" -o "%OBJ%\main.o"
if errorlevel 1 goto :error
g++ %CXXFLAGS% -c "%ROOT%\libraries\imgui\imgui_demo.cpp" -o "%OBJ%\imgui_demo.o"
if errorlevel 1 goto :error
g++ %CXXFLAGS% -c "%ROOT%\libraries\imgui\backends\imgui_impl_sdl2.cpp" -o "%OBJ%\imgui_impl_sdl2.o"
if errorlevel 1 goto :error
g++ %CXXFLAGS% -c "%ROOT%\libraries\imgui\backends\imgui_impl_opengl3.cpp" -o "%OBJ%\imgui_impl_opengl3.o"
if errorlevel 1 goto :error

echo [exe] compiling resources...
windres -I "." res.rc -O coff -o "%OBJ%\res.o"
if errorlevel 1 goto :error

echo [exe] linking...
g++ -O2 -o "%OUT%\lite.exe" ^
    "%OBJ%\main.o" ^
    "%OBJ%\imgui_demo.o" ^
    "%OBJ%\imgui_impl_sdl2.o" ^
    "%OBJ%\imgui_impl_opengl3.o" ^
    "%OBJ%\res.o" ^
    -L"%OUT%" -llite_binding ^
    -L"%SDL%\lib" -lmingw32 -lSDL2main -lSDL2 ^
    -lopengl32 -lgdi32 -limm32 -lole32 -loleaut32 -luuid -lwinmm ^
    -static-libgcc -static-libstdc++ -mwindows
if errorlevel 1 goto :error

echo [exe] staging runtime files...
copy /Y "%SDL%\bin\SDL2.dll" "%OUT%\SDL2.dll" >nul
if errorlevel 1 goto :error
if exist "%OUT%\data" rd /s /q "%OUT%\data"
xcopy /E /I /Y /Q "%ROOT%\src\lua" "%OUT%\data" >nul
if errorlevel 1 goto :error

echo [exe] done: %OUT%\lite.exe
exit /b 0

:error
echo [exe] build failed
exit /b 1
