@echo off
rem ========================================================================================
rem  PsBufferPaint specimen-sheet demo build script.
rem
rem  Usage:  build.bat          builds main.exe
rem          build.bat clean    deletes build output
rem
rem  Run the geometry self-test with:  set PSBUFFERPAINT_SELFTEST=1 && main.exe
rem ========================================================================================

setlocal

rem fbc is not on PATH in this workspace; the toolchain is vendored under tiko_editor.
set FBC="C:\dev\tiko_editor\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe"

rem Sources include AfxNova as "AfxNova\...", i.e. relative to the workspace root, so the
rem include path must be C:\dev or every AfxNova include fails to resolve.
set INCLUDE_ROOT="C:\dev"

rem main.rc embeds main_manifest.xml as resource type 24 (RT_MANIFEST). That manifest is what
rem makes the process DPI-AWARE. Without it Windows reports 96 DPI whatever the user's scaling
rem is and bitmap-stretches the window -- which silently invalidates every geometry assertion
rem that derives a scaled pen width. fbc compiles a .rc listed on the command line and links
rem the result automatically.
set RESOURCE=main.rc

if /i "%~1"=="clean" (
    if exist main.exe del main.exe
    if exist main.o   del main.o
    if exist main.obj del main.obj
    echo Cleaned.
    goto :eof
)

if not exist %FBC% (
    echo ERROR: fbc64.exe not found at %FBC%
    exit /b 1
)

if not exist %RESOURCE% (
    echo ERROR: %RESOURCE% not found -- the app would build DPI-unaware and the
    echo        PaintLine assertions would pass for the wrong reason.
    exit /b 1
)

echo Building main.exe
%FBC% -i %INCLUDE_ROOT% main.bas %RESOURCE%
if errorlevel 1 (
    echo.
    echo BUILD FAILED.
    exit /b 1
)
echo Build OK.

endlocal
