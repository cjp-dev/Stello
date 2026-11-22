@echo off
setlocal enabledelayedexpansion

set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"
for %%i in ("%HERE%\..\..\..") do set "REPO=%%~fi"

if "%~1"=="" (
    set "EGAROUCID=%REPO%\Egaroucid-main"
) else (
    set "EGAROUCID=%~1"
)

if not exist "%EGAROUCID%\src\engine\engine_all.hpp" (
    echo Egaroucid source not found in "%EGAROUCID%" 1>&2
    exit /b 1
)

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
        echo Could not locate Visual Studio C++ build tools ^(vcvars64.bat^). 1>&2
        exit /b 1
    )
    call "!STELLO_VS_INSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
)

if defined STELLO_LLVM_BIN (
    if exist "%STELLO_LLVM_BIN%\clang++.exe" set "PATH=%STELLO_LLVM_BIN%;%PATH%"
) else (
    if exist "%ProgramFiles%\LLVM\bin\clang++.exe" set "PATH=%ProgramFiles%\LLVM\bin;%PATH%"
)

where clang++.exe >nul 2>nul || (echo Could not find clang++.exe 1>&2 & exit /b 1)

if not exist "%HERE%\bin" mkdir "%HERE%\bin"

echo Building egaroucid-runner.exe ...
clang++ -std=c++20 -O3 -DNDEBUG -march=native -flto -fuse-ld=lld -DHAS_AVX512 -DHAS_AMD_PROCESSOR -w ^
  -I"%EGAROUCID%\src" "%HERE%\runner.cpp" -o "%HERE%\bin\egaroucid-runner.exe" || exit /b 1

echo Done: "%HERE%\bin\egaroucid-runner.exe"
