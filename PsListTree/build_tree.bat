@echo off
rem ========================================================================================
rem  PsListTree TREE demo build script (tree_demo.bas -> tree_demo.exe).
rem
rem  A focused harness for the treeview features only. See build.bat for the full-control demo.
rem
rem  Usage:  build_tree.bat          builds tree_demo.exe
rem          build_tree.bat clean    deletes build output
rem ========================================================================================

setlocal

rem fbc is not on PATH in this workspace; the toolchain is vendored under tiko_editor.
set FBC="C:\dev\tiko_editor\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe"

rem Sources include AfxNova as "AfxNova\...", i.e. relative to the workspace root.
set INCLUDE_ROOT="C:\dev"

rem main.rc embeds main_manifest.xml (RT_MANIFEST) -- the same DPI-aware manifest the other
rem demo uses. Without it the process is DPI-unaware and the twisty/indent geometry would be
rem bitmap-stretched at scaled DPI.
set RESOURCE=main.rc

if /i "%~1"=="clean" (
    if exist tree_demo.exe del tree_demo.exe
    if exist tree_demo.o   del tree_demo.o
    if exist tree_demo.obj del tree_demo.obj
    echo Cleaned.
    goto :eof
)

if not exist %FBC% (
    echo ERROR: fbc64.exe not found at %FBC%
    exit /b 1
)

if not exist %RESOURCE% (
    echo ERROR: %RESOURCE% not found -- the app would build DPI-unaware.
    exit /b 1
)

echo Building tree_demo.exe
%FBC% -i %INCLUDE_ROOT% tree_demo.bas %RESOURCE% -x tree_demo.exe
if errorlevel 1 (
    echo.
    echo BUILD FAILED.
    exit /b 1
)
echo Build OK.

endlocal
