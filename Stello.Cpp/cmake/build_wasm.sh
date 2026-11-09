#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 4 ]; then
    echo "Usage: build_wasm.sh <source-dir> <build-dir> <out-dir> <single|threaded> [ON|OFF]" >&2
    exit 1
fi

STELLO_SRC="$(cd "$1" && pwd)"
mkdir -p "$2" "$3"
STELLO_BLD="$(cd "$2" && pwd)"
STELLO_OUT="$(cd "$3" && pwd)"
STELLO_MODE="$4"
STELLO_ABORT_EX="${5:-ON}"

case "$STELLO_MODE" in
    threaded) STELLO_THREADS_OPT=ON ;;
    single)   STELLO_THREADS_OPT=OFF ;;
    *)
        echo "Unknown mode '$STELLO_MODE' (expected single or threaded)." >&2
        exit 1
        ;;
esac

if ! command -v emcmake >/dev/null 2>&1; then
    for candidate in "${STELLO_EMSDK:-}" "${EMSDK:-}" "$HOME/emsdk"; do
        if [ -n "$candidate" ] && [ -f "$candidate/emsdk_env.sh" ]; then
            EMSDK_QUIET=1 source "$candidate/emsdk_env.sh" >/dev/null
            break
        fi
    done
fi

if ! command -v emcmake >/dev/null 2>&1; then
    echo "Could not locate Emscripten (emcmake). Install emsdk or set StelloEmsdk / EMSDK." >&2
    exit 2
fi

emcmake cmake -S "$STELLO_SRC" -B "$STELLO_BLD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -flto" \
  "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=-O3 -flto" \
  "-DSTELLO_ABORT_EXCEPTIONS=${STELLO_ABORT_EX}" \
  -DSTELLO_BUILD_SHARED=OFF \
  -DSTELLO_BUILD_CLI=OFF \
  -DSTELLO_BUILD_TESTS=OFF \
  -DSTELLO_BUILD_WASM=ON \
  "-DSTELLO_WASM_THREADS=${STELLO_THREADS_OPT}" \
  "-DSTELLO_WASM_OUT_DIR=${STELLO_OUT}"

cmake --build "$STELLO_BLD" --target stello_wasm_stage
