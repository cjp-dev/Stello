@echo off
setlocal enabledelayedexpansion

set "PATH=C:\Program Files\LLVM\bin;%PATH%"
where cmake.exe >nul 2>nul
if errorlevel 1 (
    set "STELLO_VS_INSTALL="
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
            set "STELLO_VS_INSTALL=%%i"
        )
    )
    if defined STELLO_VS_INSTALL (
        call "!STELLO_VS_INSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
    )
)

set "SRC=%~dp0.."
for %%I in ("%SRC%") do set "SRC=%%~fI"
set "GEN_DIR=%SRC%\build\clang-pgo-gen"
set "USE_DIR=%SRC%\build\clang-pgo-use"
set "PROF_RAW=%GEN_DIR%\stello.profraw"
set "PROF_DATA=%GEN_DIR%\stello.profdata"

cmake -S "%SRC%" -B "%GEN_DIR%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_CXX_COMPILER=clang++ ^
  -DCMAKE_AR=llvm-ar ^
  -DCMAKE_RANLIB=llvm-ranlib ^
  "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -flto" ^
  "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=-flto -fuse-ld=lld" ^
  "-DCMAKE_SHARED_LINKER_FLAGS_RELEASE=-flto -fuse-ld=lld" ^
  -DSTELLO_BUILD_TESTS=OFF ^
  -DSTELLO_PGO_GENERATE=ON || exit /b 1

cmake --build "%GEN_DIR%" --parallel 8 --target stello-cpp || exit /b 1

del /q "%GEN_DIR%\*.profraw" 2>nul
set "LLVM_PROFILE_FILE=%PROF_RAW%"
"%GEN_DIR%\stello-cpp.exe" solve-bench --from 40 --to 44 --passes 1 || exit /b 1
set "LLVM_PROFILE_FILE=%GEN_DIR%\stello2.profraw"
"%GEN_DIR%\stello-cpp.exe" mid-bench --depth 12 --passes 1 || exit /b 1

llvm-profdata merge -output="%PROF_DATA%" "%PROF_RAW%" "%GEN_DIR%\stello2.profraw" || exit /b 1
del /q "%PROF_RAW%" "%GEN_DIR%\stello2.profraw" 2>nul

cmake -S "%SRC%" -B "%USE_DIR%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_CXX_COMPILER=clang++ ^
  -DCMAKE_AR=llvm-ar ^
  -DCMAKE_RANLIB=llvm-ranlib ^
  "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -flto" ^
  "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=-flto -fuse-ld=lld" ^
  "-DCMAKE_SHARED_LINKER_FLAGS_RELEASE=-flto -fuse-ld=lld" ^
  -DSTELLO_BUILD_TESTS=OFF ^
  "-DSTELLO_PGO_USE=%PROF_DATA%" || exit /b 1

cmake --build "%USE_DIR%" --parallel 8 --target stello-cpp stello_engine || exit /b 1
echo Clang PGO build ready in "%USE_DIR%".
