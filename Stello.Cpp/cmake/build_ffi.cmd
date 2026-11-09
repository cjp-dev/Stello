@echo off
setlocal enabledelayedexpansion

if "%~1"=="" (
    echo Usage: build_ffi.cmd ^<source-dir^> ^<build-dir^> 1>&2
    exit /b 1
)
if "%~2"=="" (
    echo Usage: build_ffi.cmd ^<source-dir^> ^<build-dir^> 1>&2
    exit /b 1
)

set "STELLO_SRC=%~1"
set "STELLO_BLD=%~2"

if not defined VSCMD_VER (
    set "STELLO_VS_INSTALL="
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
            set "STELLO_VS_INSTALL=%%i"
        )
    )
    if not defined STELLO_VS_INSTALL (
        for %%p in ("%ProgramFiles(x86)%\Microsoft Visual Studio\2022" "%ProgramFiles%\Microsoft Visual Studio\2022") do (
            for %%e in (BuildTools Community Professional Enterprise) do (
                if not defined STELLO_VS_INSTALL if exist "%%~p\%%e\VC\Auxiliary\Build\vcvars64.bat" (
                    set "STELLO_VS_INSTALL=%%~p\%%e"
                )
            )
        )
    )
    if not defined STELLO_VS_INSTALL (
        echo Could not locate Visual Studio C++ build tools ^(vcvars64.bat^). Install the "Desktop development with C++" workload or VS 2022 BuildTools. 1>&2
        exit /b 1
    )
    call "!STELLO_VS_INSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
)

if defined STELLO_LLVM_BIN (
    if exist "%STELLO_LLVM_BIN%\clang++.exe" set "PATH=%STELLO_LLVM_BIN%;%PATH%"
) else (
    if exist "%ProgramFiles%\LLVM\bin\clang++.exe" set "PATH=%ProgramFiles%\LLVM\bin;%PATH%"
)

where clang++.exe >nul 2>nul
if errorlevel 1 (
    echo Could not find clang++.exe ^(LLVM^) on PATH or in "%ProgramFiles%\LLVM\bin". Install LLVM or set StelloLlvmBin. 1>&2
    exit /b 1
)
where llvm-ar.exe >nul 2>nul
if errorlevel 1 (
    echo Could not find llvm-ar.exe on PATH or in "%ProgramFiles%\LLVM\bin". 1>&2
    exit /b 1
)
where cmake.exe >nul 2>nul
if errorlevel 1 (
    echo Could not find cmake.exe on PATH or in Visual Studio C++ CMake tools. 1>&2
    exit /b 1
)
where ninja.exe >nul 2>nul
if errorlevel 1 (
    echo Could not find ninja.exe on PATH or in Visual Studio C++ CMake tools. 1>&2
    exit /b 1
)

cmake -S "%STELLO_SRC%" -B "%STELLO_BLD%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_CXX_COMPILER=clang++ ^
  -DCMAKE_AR=llvm-ar ^
  -DCMAKE_RANLIB=llvm-ranlib ^
  "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -flto" ^
  "-DCMAKE_SHARED_LINKER_FLAGS_RELEASE=-flto -fuse-ld=lld" ^
  -DSTELLO_ABORT_EXCEPTIONS=ON ^
  -DSTELLO_BUILD_SHARED=ON ^
  -DSTELLO_BUILD_TESTS=OFF || exit /b 1

cmake --build "%STELLO_BLD%" --target stello_engine || exit /b 1
