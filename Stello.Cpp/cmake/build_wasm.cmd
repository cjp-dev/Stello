@echo off
setlocal enabledelayedexpansion

if "%~4"=="" (
    echo Usage: build_wasm.cmd ^<source-dir^> ^<build-dir^> ^<out-dir^> ^<single^|threaded^> [ON^|OFF] 1>&2
    exit /b 1
)

set "STELLO_SRC=%~f1"
set "STELLO_BLD=%~f2"
set "STELLO_OUT=%~f3"
set "STELLO_MODE=%~4"
set "STELLO_ABORT_EX=%~5"
if "%STELLO_ABORT_EX%"=="" set "STELLO_ABORT_EX=ON"

if /I "%STELLO_MODE%"=="threaded" (
    set "STELLO_THREADS_OPT=ON"
) else if /I "%STELLO_MODE%"=="single" (
    set "STELLO_THREADS_OPT=OFF"
) else (
    echo Unknown mode "%STELLO_MODE%" ^(expected single or threaded^). 1>&2
    exit /b 1
)

set "EMSDK_QUIET=1"
where emcmake >nul 2>nul
if errorlevel 1 (
    if defined STELLO_EMSDK if exist "%STELLO_EMSDK%\emsdk_env.bat" (
        call "%STELLO_EMSDK%\emsdk_env.bat" >nul || exit /b 1
    )
)
where emcmake >nul 2>nul
if errorlevel 1 (
    if defined EMSDK if exist "%EMSDK%\emsdk_env.bat" (
        call "%EMSDK%\emsdk_env.bat" >nul || exit /b 1
    )
)
where emcmake >nul 2>nul
if errorlevel 1 (
    if exist "%USERPROFILE%\emsdk\emsdk_env.bat" (
        call "%USERPROFILE%\emsdk\emsdk_env.bat" >nul || exit /b 1
    )
)

where emcmake >nul 2>nul
if errorlevel 1 (
    if defined EMSDK if exist "%EMSDK%\upstream\emscripten\emcmake.exe" (
        set "PATH=%EMSDK%\upstream\emscripten;%PATH%"
    )
)
where emcmake >nul 2>nul
if errorlevel 1 (
    echo Could not locate Emscripten ^(emcmake^). Install emsdk or set StelloEmsdk / EMSDK. 1>&2
    exit /b 2
)

where ninja.exe >nul 2>nul
if errorlevel 1 set "_NEED_VS=1"
where cmake.exe >nul 2>nul
if errorlevel 1 set "_NEED_VS=1"
if defined _NEED_VS if not defined VSCMD_VER (
    set "STELLO_VS_INSTALL="
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
            set "STELLO_VS_INSTALL=%%i"
        )
    )
    if defined STELLO_VS_INSTALL (
        call "!STELLO_VS_INSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul
    )
)

where cmake.exe >nul 2>nul
if errorlevel 1 (
    echo Could not locate cmake.exe on PATH or in Visual Studio C++ CMake tools. 1>&2
    exit /b 1
)
where ninja.exe >nul 2>nul
if errorlevel 1 (
    echo Could not locate ninja.exe on PATH or in Visual Studio C++ CMake tools. 1>&2
    exit /b 1
)

call emcmake cmake -S "%STELLO_SRC%" -B "%STELLO_BLD%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -flto" ^
  "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=-O3 -flto" ^
  -DSTELLO_ABORT_EXCEPTIONS=%STELLO_ABORT_EX% ^
  -DSTELLO_BUILD_SHARED=OFF ^
  -DSTELLO_BUILD_CLI=OFF ^
  -DSTELLO_BUILD_TESTS=OFF ^
  -DSTELLO_BUILD_WASM=ON ^
  -DSTELLO_WASM_THREADS=%STELLO_THREADS_OPT% ^
  "-DSTELLO_WASM_OUT_DIR=%STELLO_OUT%" || exit /b 1

cmake --build "%STELLO_BLD%" --target stello_wasm_stage || exit /b 1
