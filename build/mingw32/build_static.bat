@echo off
rem Builds the portable lite binding (Lua + Dear ImGui + src\bindings) as a
rem static library. The host application supplies the ImGui platform/renderer
rem backends and calls the Lite API declared in src\bindings\Lite.h.
rem
rem Output (relative to this directory):
rem   dist\liblite_binding.a
rem   dist\include\Lite.h

setlocal EnableDelayedExpansion
cd /d "%~dp0"

rem The build runs from this directory, so every path is relative: this keeps
rem non-ASCII checkout paths away from the linker's library search.
set "ROOT=..\.."
set "OUT=dist"
set "OBJ=obj\static"

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OBJ%" mkdir "%OBJ%"
if exist "%OBJ%\*.o" del /q "%OBJ%\*.o"

set "INCLUDES=-I"%ROOT%\libraries\imgui" -I"%ROOT%\libraries\lua" -I"%ROOT%\src\bindings""
set "CXXFLAGS=-std=c++17 -O2 -Wall -fno-strict-aliasing %INCLUDES%"
set "CFLAGS=-O2 -fno-strict-aliasing -DLUA_USE_POPEN -I"%ROOT%\libraries\lua""

echo [static] building lua...
for /f "delims=" %%f in ('dir /b /a-d "%ROOT%\libraries\lua\*.c"') do (
    if /I not "%%~nxf"=="lua.c" if /I not "%%~nxf"=="ltests.c" (
        gcc %CFLAGS% -c "%ROOT%\libraries\lua\%%f" -o "%OBJ%\%%~nf.o"
        if errorlevel 1 goto :error
    )
)

echo [static] building dear imgui...
for %%f in (imgui imgui_draw imgui_tables imgui_widgets) do (
    g++ %CXXFLAGS% -c "%ROOT%\libraries\imgui\%%f.cpp" -o "%OBJ%\%%f.o"
    if errorlevel 1 goto :error
)

echo [static] building lite bindings...
for /f "delims=" %%f in ('dir /b /a-d "%ROOT%\src\bindings\*.cpp"') do (
    g++ %CXXFLAGS% -c "%ROOT%\src\bindings\%%f" -o "%OBJ%\%%~nf.o"
    if errorlevel 1 goto :error
)

echo [static] archiving...
if exist "%OUT%\liblite_binding.a" del "%OUT%\liblite_binding.a"
set "OBJECTS="
for /f "delims=" %%f in ('dir /b /a-d "%OBJ%\*.o"') do set "OBJECTS=!OBJECTS! "%OBJ%\%%f""
ar rcs "%OUT%\liblite_binding.a" !OBJECTS!
if errorlevel 1 goto :error

if not exist "%OUT%\include" mkdir "%OUT%\include"
copy /Y "%ROOT%\src\bindings\Lite.h" "%OUT%\include\Lite.h" >nul

echo [static] done: %OUT%\liblite_binding.a
exit /b 0

:error
echo [static] build failed
exit /b 1
