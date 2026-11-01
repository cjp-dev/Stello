# C++ against C#, natively and in WebAssembly (2026-11-01)

The question: would a C++ engine be faster than the C# one, on the desktop and in the browser? Egaroucid makes a fair test, because `Egaroucid.Engine` is a close C# translation of Egaroucid's C++ source (`Egaroucid-main`). Both search the same trees, so the times compare the languages and their compilers, not the algorithms.

## Set-up

- **Machine:** the Intel i7-12850HX laptop VM, 1 thread, nothing else running.
- **Positions:**
  - endgame: FFO #40–49, solved exactly;
  - midgame: the 100 positions of `midgame.txt`, Egaroucid level 15.
- **Settings:** no book, table of 2^21 entries, the table cleared before each position.
- **Node counts:** they agree within 2 %. For example, FFO #40–49 takes 518.9 M nodes in C# (native and WASM alike) and 520.9 M in C++.
- **Builds:**

| Build | How |
|---|---|
| C++ native, AVX2 | `g++ 15.2 -O2 -march=native` (Egaroucid's own build line) |
| C++ native, no AVX2 | `g++ -O2 -march=x86-64-v2 -DHAS_NO_AVX2`: POPCNT, but no AVX2 or BMI2 |
| C++ WebAssembly | Emscripten 6.0.13, `em++ -O3 -DHAS_NO_AVX2`, with and without `-msimd128`, run in Node 22 (V8, the engine of Chrome) |
| C# native | .NET 10 JIT, Release (the port uses portable bit operations, no intrinsics) |
| C# WebAssembly | .NET 10 `browser-wasm` with AOT (`RunAOTCompilation`, `WasmEnableSIMD`), as the web app is built, run in Node 22 |

- **Changes for WebAssembly:** the C++ console program needed two one-line changes, made on a copy of the source:
  - `linux/limits.h` replaced by `limits.h`;
  - one hash-size constant that does not fit a 32-bit `size_t`.

## Results

| Build | FFO #40–49 | Nodes/s | Midgame, level 15 | Nodes/s |
|---|---:|---:|---:|---:|
| C++ native, AVX2 | **11.6 s** | 45.0 M | **12.5 s** | 22.2 M |
| C++ native, no AVX2 | 17.1 s | 30.5 M | 18.8 s | 15.1 M |
| C++ WebAssembly + SIMD128 | 17.7 s | 29.5 M | 18.8 s | 15.0 M |
| C++ WebAssembly | 18.3 s | 28.8 M | 19.3 s | 14.7 M |
| C# native (.NET 10) | 26.8 s | 19.4 M | 29.0 s | 9.7 M |
| C# WebAssembly (AOT) | 34.2 s | 15.2 M | 50.5 s | 5.6 M |

Speed-ups of C++ over C#:

| | Endgame | Midgame |
|---|---:|---:|
| Native, both without AVX2 | 1.57× | 1.54× |
| Native, C++ with AVX2 | 2.3× | 2.3× |
| WebAssembly | 1.9× | 2.7× |

## What it shows

- **C++ in WebAssembly is as fast as native C++ without AVX2.** V8 compiles the 64-bit bit operations well, so the cost of the browser is only the missing AVX2/BMI2, not WebAssembly itself. SIMD128 adds about 3 %.
- **C# loses more in WebAssembly than C++ does:** 1.3× slower than native C# in the endgame and 1.7× slower in the midgame. The evaluation-heavy midgame suffers most.
- **In the browser, C++ in WebAssembly beats native C#:** 17.7 s against 26.8 s in the endgame, 18.8 s against 29.0 s in the midgame.
- **On the desktop, C++ gains 1.5× from the compiler and 2.3× with AVX2.** C# could use AVX2 too (`System.Runtime.Intrinsics`), which might close part of that gap. The port does not use intrinsics, so this was not measured.
- **Not measured:**
  - threads: WebAssembly threads are mature in Emscripten, but experimental in .NET's WebAssembly runtime;
  - other browsers: Firefox and Safari have their own WebAssembly compilers;
  - Clang for native builds, which Egaroucid's author finds faster than g++.

## Reproducing

Nothing here is in the repository. Egaroucid's source and resources are not committed, and the binaries were built in a scratch folder.

1. Build the C++ versions with the build lines above, from a copy of `Egaroucid-main/src`.
2. Run them with `<binary> -l 60 -t 1 -nobook -hash 21 -solve <problems>`, where each problem line is `<64 squares> <X|O>`. For the midgame use `-l 15`.
3. The C# side is a 40-line `Program.cs`. It loads the resources, then calls `Ai.Search(context, board, level, useMultiThread: false)` for each line.
   - For WebAssembly, it is built in a `Microsoft.NET.Sdk.WebAssembly` project.
   - In Node, a small `main.mjs` copies the resources into the WebAssembly file system before `runMain`.
