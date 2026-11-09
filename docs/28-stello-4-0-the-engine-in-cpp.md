# 28 – Stello 4.0: the engine in C++

[Back to the index](README.md)

## In short

Stello 4.0 ports Stello 3.1's Rust search core ([`Stello.Rust`](../Stello.Rust)) **node for node into modern C++20** ([`Stello.Cpp`](../Stello.Cpp)), integrates it behind the existing .NET C ABI (`stello_engine.dll` / `libstello_engine.so`) and into Emscripten WebAssembly for the web app, and asks a clean engineering question: **is an identical Othello engine faster in C++20 than in Rust?**

The plan ([Stello 4.0 C++ implementation.md](../Plans/Stello%204.0%20C++%20implementation.md)) ran in two parts on Windows x64 (**AMD Ryzen 7 7800X3D**, 8 cores / 16 threads), with all measurements and parity gates logged in [Stello 4.0 C++ findings.md](../Plans/Stello%204.0%20C++%20findings.md):

- **Zero algorithmic drift:** on 1 thread, `Stello.Cpp` returns the exact same evaluation checksums, moves, scores, score kinds and node counts as Stello 3.1 across all 105 strict signatures (`3,716,389` midgame d10 nodes; `93,697,164` FFO #40–44 nodes), FFO #40–49 (`1,115,666,701` nodes), and >31.7 billion nodes of midgame and endgame parity checks.
- **Zero external dependencies and sanitizer clean:** a self-contained ~250-line RFC 1951/1952 gzip decompressor (`eval/gzip.cpp`) unpacks the embedded weights; `clang-sanitized` (ASan + UBSan) passes every parity, search and 8-thread seqlock test with zero findings.
- **Where strict 1-to-1 LLVM tied, and what actually sped it up:** because `rustc 1.99.1` and `clang++ 23.1.5` share LLVM 23.1, baseline x86-64 C++ with tagged `AbortResult` returns tied Rust in the 1-thread endgame solver (`+0.1%`). Switching `STELLO_ABORT_EXCEPTIONS=ON` (zero-cost table-driven `throw Abort` instead of per-call tag branches) gained **+5.6–6.0%** in FFO #40–49 without changing a single node. Adding `#pragma clang loop vectorize(disable)` in `flips()` cured an AVX-512 `zmm` SLP regression under `-march=native` (`perft(8)` **+71.8%**), while keeping tuned `clang-release` as the `.NET` FFI default avoids the remaining `zmm` downclocking in solver `legal_moves()`.
- **Native `.NET`, 1,180-game match, and WebAssembly speed:** through `.NET` FFI (`engine=stello` vs preserved Rust 3.1 `engine=stello-v3`), C++ 4.0 is **+4.3%** faster on 1t midgame depth 14, **+5.3%** faster on 1t FFO #40–49, and **+3.0–3.8%** faster on 8t FFO #40–49. In a full 1,180-game timed match at 500 ms/move (`match_p5-cpp-vs-v3-500ms`), C++ 4.0 scored `591–589` (`50.1%` vs `49.9%`, Elo `+1` / no significant difference) with **+7.6% higher average speed** (`16.58M` vs `15.41M` nps). Compiled with Emscripten (`-fwasm-exceptions` and `-pthread`), C++ WebAssembly drops the Rust nightly toolchain requirement, shrinks the compressed `.wasm` by **20.5–25.4 KB (brotli)**, and beats Rust 3.1 WASM across every tested Node configuration (**+6.8% to +17.3%** on 1 thread, **+3.6% to +15.8%** on 4 and 8 threads).

## The question and the C++20 core

### From Rust 3.1 to `Stello.Cpp`

`Stello C++` in the repository root is the original 1992–1998 MFC program. The new C++20 search engine lives alongside `Stello.Net` and `Stello.Rust` in [`Stello.Cpp`](../Stello.Cpp) (namespace `stello`, C++20, `-Wall -Wextra -Wpedantic` / `/W4 /permissive-`, 0 warnings).

Every module of `stello-core` maps 1-to-1 to C++20 headers and sources ([chapter 01](01-overview.md#the-engine-at-a-glance)):

- **Board, CPUID and evaluation (`src/core/`, `src/eval/`):** `Board`, `Square`, `SquareMask`, `bits`, runtime CPUID BMI2 detection (`cpu.cpp`, isolating `_pext_u64` in `patterns_bmi2.cpp` so baseline builds keep only two `pextq` instructions binary-wide), 8 symmetries, `plus15` weight folding, and `eval/gzip.cpp` — a self-contained multi-member RFC 1951/1952 DEFLATE/gzip decompressor with CRC-32 and ISIZE verification unpacking `stello-eval.bin` (`990,568 -> 4,673,605` bytes, matching `flate2` byte for byte).
- **Midgame search, TT, pool, YBWC and endgame solver (`src/search/`, `src/endgame/`):** Iterative deepening PVS, Multi-ProbCut, response ordering, 64-byte cache-line aligned seqlocked hash tables (`Slot` with explicit `padding_` avoiding MSVC `warning C4324`), 3.1's `root_empties` aging and unified `count_node()` / terminal evaluation counting, `ThreadPool` with lock-free `idle` refusal and `WaitOnDrop`, Lazy SMP helpers, YBWC split chains, `LocalTable`, edge stability tables, and `last4`–`last1`.
- **Two subtle platform traps caught early:**
  - On Windows Clang, MSVC `<atomic>` tags `_Atomic_storage` with `gnu::target("cx16")`; without `-mcx16`, Clang refused to inline 1-byte relaxed loads inside `EvalWeights` leaf evaluation and emitted an out-of-line call per feature. Adding `-mcx16` to Win64 non-Emscripten Clang folded those loads to plain `movb`.
  - Under ASan (`STELLO_SANITIZE=ON`), `vcvars64.bat` prepends Visual Studio's older `clang_rt.asan_dynamic-x86_64.dll` onto `PATH`. `Stello.Cpp/CMakeLists.txt` copies LLVM 23.1.5's matching ASan runtime beside each sanitized test executable at post-build.

### Five C++ undefined-behavior and parity guards

Matching a tuned `-O3 -flto` Rust search node for node under Clang, MSVC and UBSan requires preventing five subtle C++ traps ([the plan](../Plans/Stello%204.0%20C++%20implementation.md#critical-c-undefined-behavior--parity-guards)):

1. **Wrapping signed arithmetic:** Rust's `wrapping_add` wraps two's-complement integers, whereas signed integer overflow is UB in C++. `MoveOrdering::record_response` (`int32_t`) and `EvalWeights::read` delta unfolding (`int16_t`) compute sums through explicit unsigned casts.
2. **Masked shift counts:** In `stable::play_on_line`, `i` reaches `-1` when `place == 0`, and in `ybwc::split`, all 64 bits can be masked (`children.size() == 64`). Shifting by `< 0` or `>= 64` is UB in C++, so both mask explicitly (`i & 31`, `~uint64_t{0}`).
3. **Argument evaluation order:** Rust guarantees left-to-right evaluation of function arguments; C++ leaves argument order unspecified. Side-effecting calls (`ordering_value`, `last1`, which increment `nodes` and `evaluations`) are always evaluated into local temporaries before calling `std::min`, `std::max`, or parent functions so tie-sensitive node limits cancel cleanly.
4. **Unified 3.1 counting:** `Searcher::count_node()` checks `abort_flag` at exact 1,024-node intervals across every recursive solver and `last4`–`last1` path, and terminal game-over scores count `evaluations += 1` identically to Stello 3.1.
5. **64-byte seqlock cache lines:** `TranspositionTable` allocates via `::operator new[](count * sizeof(Slot), std::align_val_t{64})` and zeroes slots before use, avoiding false sharing across worker cores.

## Controlled benchmarks: Rust vs Clang vs MSVC

Phase 4 tested `Stello.Rust` 3.1 (`rustc 1.99.1`, LLVM 23.1.1) head-to-head against standalone `stello-cpp` builds under Clang `23.1.5` and MSVC `19.44` across baseline (`x86-64` + runtime BMI2 `pext`), `--cpu portable` (SWAR LUT extraction), native Zen 4 (`-march=native` / `target-cpu=native` / `/arch:AVX2`), and `STELLO_ABORT_EXCEPTIONS=OFF` (`AbortResult`) vs `ON` (`throw Abort`). Every 1-thread run kept exact Stello 3.1 node counts (`5` passes for `perft`/`eval-split`, `3` passes for search/FFO, `--hash-bits 19`, idle Ryzen 7 7800X3D).

### Baseline vs exceptions vs native Zen 4 (Phase 4 & Step 5.1)

Negative `Δ` in these tables means slower than `rust-base`; positive `+X%` means faster (computed on mean wall time, `(rust_mean / cfg_mean - 1)`, and on mean `M nps` where noted):

| Configuration | Compiler & Mode | `perft(8)` (ms) | `mid d10` 1t | `mid d12` 1t | `mid d16` 1t | `FFO #40–49` 1t | `FFO #40–49` 8t | `FFO #40–49` 16t |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| `rust-base` | `rustc 1.99.1`, `Result`, x86-64+`pext` | 4.26 ms | 0.381 s | 1.068 s | 7.542 s | 23.845 s (46.8M) | 6.431 s (253.7M) | 5.691 s (304.7M) |
| `rust-native` | `rustc 1.99.1`, `target-cpu=native` | 3.66 ms (+16.4%) | 0.381 s (-0.1%) | 1.043 s (+2.4%) | 7.521 s (+0.3%) | 23.167 s (+2.9%) | 6.113 s (+5.2%) | 4.889 s (+16.4%) |
| `clang-base` (`OFF`) | `clang++ 23.1.5`, `AbortResult`, x86-64 | 4.58 ms (-7.0%) | 0.367 s (+3.8%) | 1.008 s (+5.9%) | 7.445 s (+1.3%) | 23.823 s (+0.1%) | 6.425 s (+0.1%) | 5.512 s (+3.2%) |
| `clang-base-exc` (`ON`) | `clang++ 23.1.5`, `throw Abort`, x86-64 | — | 0.361 s (+5.3%) | 1.000 s (+6.7%) | 7.340 s (+2.7%) | **22.566 s (+5.7%)** | **6.038 s (+6.5%)** | 5.420 s (+5.0%) |
| `clang-native` (Phase 4 `OFF`) | `clang++ 23.1.5`, `-march=native` | 7.56 ms (-43.7%) | **0.350 s (+8.7%)** | **0.978 s (+9.2%)** | **7.069 s (+6.7%)** | 24.379 s (-2.2%) | 6.436 s (-0.1%) | 5.090 s (+11.8%) |
| `clang-native` (tuned `ON`) | + scalar `flips()` pragmas, `throw Abort` | **4.40 ms** (**+71.8%** vs P4) | 0.364 s (+4.7%) | 1.029 s (+3.8%) | 7.233 s (+4.3%) | 23.599 s (+1.0%) | 6.402 s (+0.5%) | 5.316 s (+7.1%, 328.8M nps) |
| `clang-release` (tuned `.NET` `ON`) | `-O3 -flto -mcx16`, `throw Abort`, x86-64 | 4.62 ms (-8.7%) | **0.356 s (+7.0%)** | **0.998 s (+7.0%)** | **7.106 s (+6.1%)** | **22.488 s (+6.0%, 49.6M)** | **6.132 s (+4.9%, 271.4M)** | **5.307 s (+7.2%, 323.4M)** |
| `msvc-base` (`OFF`) | `cl 19.44`, `AbortResult`, x64 | 4.92 ms (-13.4%) | 0.414 s (-8.1%) | 1.179 s (-9.5%) | 8.681 s (-13.1%) | 28.849 s (-17.3%) | 7.576 s (-15.1%) | 6.386 s (-10.9%) |
| `msvc-native-exc` (`ON`) | `cl 19.44`, `/arch:AVX2`, `throw Abort` | — | 0.394 s (-3.4%) | 1.088 s (-1.9%) | 8.044 s (-6.2%) | **22.548 s (+5.8%)** | 6.039 s (+6.5%) | 5.081 s (+12.0%) |

### What actually made the difference

1. **Same LLVM backend, same `AbortResult` -> nearly a tie:** `rust-base` (LLVM 23.1.1) and `clang-base` (LLVM 23.1.5) both compile with Baseline x86-64 instructions (`popcnt=0`, `lzcnt=0`, runtime `pext` isolated in `Direct::sum_pext`) and return an 8-byte tagged `Abort` result (`RAX` in Win64). In FFO #40–49 they tied to within noise (`23.845 s` vs `23.823 s`, `+0.1%`). C++ already led `eval-split` (`16.25 vs 15.42 M pos/s`, `+5.4%`) and 1-thread midgame (`+3.8%` to `+5.9%` on `d10`/`d12`).
2. **MSVC `<bit>` dynamic dispatch vs `/arch:AVX2`:** `msvc-base` trailed Rust by 12–21% because MSVC's STL `<bit>` (`std::popcount`, `std::countl_zero`) emits an `__isa_available` branch + SWAR fallback at every call unless `/arch:AVX2` is active. With `/arch:AVX2` and C++ exceptions (`msvc-native-exc`), MSVC became as fast as Clang in endgame solving (`22.548 s` 1t, `+5.8%`), while still trailing in midgame pattern evaluation.
3. **Zero-cost `throw Abort` (`STELLO_ABORT_EXCEPTIONS=ON`):** Stello 3.1's `panic = "abort"` Rust core checks `?` (`Result<T, Abort>`) at every call and pass inside `search`, `ordering_value`, `end_*` and `last4`–`last1` (the Phase 2 Item 9 change that cost Rust ~1.4% in FFO #40–49). C++ exceptions reverse that cost: zero-cost DWARF/Win64 unwind tables keep the common non-aborting search branch completely free of tag checks (with a tiny RAII `ProbCutGuard` restoring `in_probcut`). Enabling exceptions accelerated FFO #40–49 by **+5.6% (1t)**, **+3.8% (4t)**, **+6.5% (8t)**, and **+5.0% (16t)** on baseline Clang — and **+14.1%** on `msvc-native` — with zero node differences.
4. **Disassembly caught Clang's AVX-512 `zmm` SLP regression in `flips()`:** In Phase 4, `clang-native` (`-march=native`) led midgame (`+8.7%` d10, `+9.2%` d12, `+6.7%` d16), yet ran **-43.7% slower** on `perft(8)` (`7.56 ms` vs `4.26 ms`). Inspecting `llvm-objdump -d` and `clang++ -S` showed Clang 23.1.5 SLP-vectorizing the 8 directional ray scans in [`endgame::bits::flips()`](../Stello.Cpp/src/endgame/bits.hpp) into 512-bit AVX-512 `zmm` instructions (`vpermq`, `vpternlogq`, `vplzcntq`, `vpsrlvq`, `vptestnmq`, `kmovd`, `8,275` `zmm` references binary-wide), surrounded by mask branches, GPR extracts, and `vzeroupper` around recursive calls. Annotating both 4-iteration ray loops with `#pragma clang loop vectorize(disable)` / `interleave(disable)` (and a Clang-native scalar inline asm barrier) cut `zmm` references in `board.cpp` and `endgame/last.cpp` to **0** (`0 vplzcntq` binary-wide) and improved `perft(8)` by **+71.8%** (`7.56 -> 4.40 ms`). Because `endgame::bits::legal_moves()` still triggers AVX-512 `zmm` in `endgame/search.cpp` under `-march=native`, tuned `clang-release` (`-O3 -flto` + `-mcx16` + `STELLO_ABORT_EXCEPTIONS=ON`) remained the fastest overall `.NET` default on both midgame (`+7.0%` d10/d12, `+6.1%` d16, `+3.3%` 8t d16) and endgame (`+6.0%` 1t, `+4.9%` 8t, `+7.2%` 16t).
5. **16 SMT threads on 8 Zen 4 cores:** On `midgame d16`, 16 threads add +45% Lazy SMP node overhead (`164M -> 240M`), outweighing SMT throughput (`+24%`) so 8 physical cores win wall time (`2.688 s` vs `3.185 s`). In deep FFO #40–49 endgames, YBWC keeps node overhead small (+6–8%), so 16 threads solve faster than 8 (`5.307 s` vs `6.132 s` on `clang-release`; peak **`4.840 s` / `354.8 M nps`** on `clang-native-exc`).

## `.NET` FFI integration and `stello-v3` preservation

### `stello_engine.dll` (ABI v3) and `Stello.Engine.csproj`

[`Stello.Cpp/src/ffi/stello_ffi.hpp`](../Stello.Cpp/src/ffi/stello_ffi.hpp) and [`stello_ffi.cpp`](../Stello.Cpp/src/ffi/stello_ffi.cpp) implement the exact 14 `extern "C"` ABI-3 entry points consumed by [`StelloNative.cs`](../Stello.Net/Stello.Engine/Native/StelloNative.cs), with compile-time `static_assert` layout verification (`sizeof(StelloSearchInfo) == 48`, `offsetof(depth) == 24`, `sizeof(StelloSearchResult) == 40`, `offsetof(mv) == 24`) and full `try { ... } catch (...)` exception boundaries returning `STELLO_INTERNAL_ERROR` instead of unwinding across C FFI. `stello_version()` returns `40000` (`4.0.0`), while `stello_abi_version()` stays `3`.

[`Stello.Engine.csproj`](../Stello.Net/Stello.Engine/Stello.Engine.csproj) replaces Cargo in `BuildStelloNative` with CMake + Ninja + Clang (`build_ffi.cmd` on Windows with static CRT `/MT` and `lld-link`, Clang + `lld` on POSIX) compiling `Stello.Cpp` with `-O3 -flto` and `STELLO_ABORT_EXCEPTIONS=ON`. Building `Stello.Net.slnx` produces `stello_engine.dll` (`1,373,184` bytes).

### Preserving Rust 3.1 (`engine=stello-v3`) and `.NET` FFI speed

To keep Stello 3.1 natively playable alongside C++ 4.0 inside `Stello.Benchmark`, a separately preserved Rust 3.1 build (`Stello.Rust/target/windows-v3/release/stello_engine_v3.dll`, `1,316,864` bytes, `stello_version() = 30100`, ABI v3) backs a new `engine=stello-v3` side option (and `--engine stello-v3` in `mid-bench`/`solve-bench`) via [`StelloLibraryMatchPlayer.cs`](../Stello.Net/tools/Stello.Benchmark/StelloLibraryMatchPlayer.cs).

Measured through the live `.NET` FFI wrapper (`Stello.Benchmark`, 3 interleaved passes, `--hash-bits 19`):

| Workload | Nodes (`stello` / `stello-v3`) | C++ 4.0 (`engine=stello`) min / mean | Rust 3.1 (`engine=stello-v3`) min / mean | C++ speed gain (mean) |
|---|---|---:|---:|---:|
| `mid-bench --depth 14` (1t) | 29,779,568 / 29,779,568 | **2.751 / 2.785 s** (`10.70M nps`) | 2.869 / 2.906 s (`10.25M nps`) | **+4.3%** |
| `solve-bench` FFO #40–49 (1t) | 1,115,666,701 / 1,115,666,701 | **22.600 / 22.700 s** (`49.15M nps`) | 23.900 / 23.900 s (`46.68M nps`) | **+5.3%** |
| `solve-bench` FFO #40–49 (8t) | ~1,597M / ~1,586M | **6.118 / 6.141 s** (`260.11M nps`) | 6.291 / 6.327 s (`250.70M nps`) | **+3.0%** (`+3.8%` M nps) |

In a full **1,180-game 500 ms/move match** (`--workers 8`, 30m 50s; [`match_p5-cpp-vs-v3-500ms.md`](../Plans/benchmark-results/match_p5-cpp-vs-v3-500ms.md)), C++ 4.0 scored **591 / 1,180** (`+501 =180 −499`) against Rust 3.1's **589 / 1,180** (`49.9%`, 95% interval `48.6%` to `51.3%`, Elo `-1` [`-10` to `+9`], discs `0.0 ± 0.2`) — **no significant strength difference**, while C++ 4.0 averaged **16.58M vs 15.41M nodes/s (+7.6%)** and **19.29 vs 19.21 plies** average midgame depth.

## Emscripten WebAssembly (`Stello.Web`)

### Architecture without `wasm-bindgen` or Rust nightly

[`Stello.Cpp/src/wasm/`](../Stello.Cpp/src/wasm) compiles `stello_core` with Emscripten (`-O3 -flto -fwasm-exceptions -sSTANDALONE_WASM=1 --no-entry -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=2GB`) into standalone WebAssembly modules plus a small 298-line ES module wrapper ([`stello_wasm.js`](../Stello.Cpp/src/wasm/stello_wasm.js)) exposing the exact API imported by [`engine-worker.js`](../Stello.Net/Stello.Web/wwwroot/js/engine-worker.js) and [`engine-pool-worker.js`](../Stello.Net/Stello.Web/wwwroot/js/engine-pool-worker.js): `default`, `initSync`, `version`, `init`, `search`, `threaded`, `idleThreads`, and `poolWorker`.

- **Direct JSON protocol (`protocol.cpp`):** parses and writes `EngineProtocol`'s exact JSON schema (preserving 64-bit board bitmasks as integers) and checks the shared 4-byte signal buffer at every 1,024-node abort point while timing with `performance.timeOrigin + performance.now()`.
- **Worker pool without Rust nightly (`threads.cpp`, `wasm_exports.cpp`):** `engine-mt` builds the same C++ sources with `-pthread` and `INITIAL_MEMORY=32MB` (shared up to 2 GiB). Nested pool Web Workers serialize briefly under an atomic `g_boot_lock`, allocate a 1 MiB C stack, initialize pthread/TLS state via standard Emscripten primitives (`pthread_create`, `pthread_getattr_np`, `_emscripten_thread_init`, `_emscripten_tls_init`), and enter `pool_worker()` waiting on `std::condition_variable` (`memory.atomic.wait32`). Zero Emscripten pthread proxy JS boilerplate, `wasm-bindgen-cli`, or Rust nightly `-Z build-std` toolchain is required locally or in [`.github/workflows/azure-static-web-apps.yml`](../.github/workflows/azure-static-web-apps.yml) (`setup-emsdk@v15`, `7.0.0`).

### Package sizes and browser-worker benchmarks

Compressed and uncompressed sizes measured on the shipped C++ 4.0 modules vs. Stello 3.1 Rust WASM packages (`node` `zlib`):

| Artifact | C++ 4.0 raw / gz / br (bytes) | Rust 3.1 raw / gz / br (bytes) | Size change (C++ vs Rust) |
|---|---:|---:|---:|
| `engine/stello_wasm.js` (ST) | 10,011 / 2,639 / 2,360 | 10,250 / 3,102 / 2,732 | **-239 B** raw / **-372 B** br |
| `engine/stello_wasm_bg.wasm` (ST) | **1,174,429** / 1,062,199 / **1,048,704** | 1,226,794 / 1,089,533 / 1,074,129 | **-52,365 B** raw / **-25,425 B** br |
| `engine-mt/stello_wasm.js` (MT) | 10,011 / 2,639 / 2,360 | 11,405 / 3,429 / 3,016 | **-1,394 B** raw / **-656 B** br |
| `engine-mt/stello_wasm_bg.wasm` (MT) | **1,203,076** / 1,074,438 / **1,058,234** | 1,251,378 / 1,094,741 / 1,078,725 | **-48,302 B** raw / **-20,491 B** br |

Paired C++ ST `STELLO_ABORT_EXCEPTIONS=ON` vs `OFF` under `-fwasm-exceptions` stays unified (`ON` saves `4,270` B `.wasm` and runs +2.0% faster on FFO #40–44, while `OFF` is +4.8–5.2% faster on midgame d10/d12 — both beat Rust 3.1 ST). Head-to-head in Node `v24.19.0` (3 passes, `--hash-bits 19`, 1t node counts `3,716,389` d10 / `10,579,515` d12 / `93,697,164` FFO #40–44 exact, 4t/8t FFO scores 5/5 exact):

| Package & Threads | Workload | C++ 4.0 mean (`M nps`) | Rust 3.1 mean (`M nps`) | C++ speed gain (wall / M nps) |
|---|---|---:|---:|---:|
| Single (`ST`) 1t | `mid-d10` (100 pos) | **789.0 ms** (4.71 M) | 842.4 ms (4.41 M) | **+6.8%** |
| Single (`ST`) 1t | `mid-d12` (100 pos) | **2,208.0 ms** (4.79 M) | 2,367.5 ms (4.47 M) | **+7.2%** |
| Single (`ST`) 1t | `FFO #40–44` (5 pos) | **2,419.4 ms** (38.73 M) | 2,797.6 ms (33.49 M) | **+15.6%** |
| Threaded (`MT`) 1t | `mid-d10` (100 pos) | **803.7 ms** (4.62 M) | 935.9 ms (3.97 M) | **+16.4%** |
| Threaded (`MT`) 1t | `mid-d12` (100 pos) | **2,225.0 ms** (4.75 M) | 2,609.6 ms (4.05 M) | **+17.3%** |
| Threaded (`MT`) 1t | `FFO #40–44` (5 pos) | **2,420.1 ms** (38.72 M) | 2,564.8 ms (36.53 M) | **+6.0%** |
| Threaded (`MT`) 4t | `mid-d10` (100 pos) | **562.2 ms** (13.51 M) | 622.9 ms (12.01 M) | **+10.8%** (`+12.5%` nps) |
| Threaded (`MT`) 4t | `mid-d12` (100 pos) | **1,391.5 ms** (14.53 M) | 1,532.7 ms (12.89 M) | **+10.1%** (`+12.7%` nps) |
| Threaded (`MT`) 4t | `FFO #40–44` (5 pos) | **1,030.7 ms** (104.50 M) | 1,067.5 ms (99.10 M) | **+3.6%** (`+5.4%` nps) |
| Threaded (`MT`) 8t | `mid-d10` (100 pos) | **464.4 ms** (22.06 M) | 537.9 ms (19.63 M) | **+15.8%** (`+12.4%` nps) |
| Threaded (`MT`) 8t | `mid-d12` (100 pos) | **1,132.6 ms** (23.56 M) | 1,232.2 ms (21.28 M) | **+8.8%** (`+10.7%` nps) |
| Threaded (`MT`) 8t | `FFO #40–44` (5 pos) | **691.5 ms** (151.27 M) | 724.8 ms (145.65 M) | **+4.8%** (`+3.9%` nps) |

Whereas Rust's `-Z build-std` threaded build lost ~10–11% in 1-thread midgame relative to Rust ST, Emscripten's `-pthread` C++ build stays within ~1.8% of C++ ST and outpaces Rust `MT` cleanly at 1, 4, and 8 threads. Live browser verification in Edge/Chrome/Firefox/Safari after deployment remains a manual step.

## Version and release checks

- [`Stello.Cpp/CMakeLists.txt`](../Stello.Cpp/CMakeLists.txt) and [`version.hpp`](../Stello.Cpp/src/core/version.hpp) report `4.0.0` (`40000`), keeping ABI version `3`.
- [`AboutInfo.cs`](../Stello.Net/Stello.App/Models/AboutInfo.cs) reports **Stello Version 4.0** and updates the shared description to cover the 1992 C++ original, the 2026 C#/.NET 10 port, the Rust 3.0 rewrite, and Stello 4.0's C++20 search core.
- All C++, .NET, WebAssembly, and Release publish checks pass with 0 warnings at Gate P7 ([findings](../Plans/Stello%204.0%20C++%20findings.md#phase-7-documentation--stello-40-release-done-2026-11-09)).

## Where in the code

| File | Role |
|---|---|
| [`Stello.Cpp/CMakeLists.txt`](../Stello.Cpp/CMakeLists.txt), [`CMakePresets.json`](../Stello.Cpp/CMakePresets.json) | C++20 targets (`stello_core`, `stello_engine`, `stello-cpp`, `stello_wasm`, `stello_tests`), 5 build presets, `-mcx16`, `STELLO_ABORT_EXCEPTIONS=ON` default, Emscripten flags |
| [`Stello.Cpp/src/core/`](../Stello.Cpp/src/core), [`eval/`](../Stello.Cpp/src/eval), [`search/`](../Stello.Cpp/src/search), [`endgame/`](../Stello.Cpp/src/endgame) | 1-to-1 C++20 engine core, self-contained `gzip.cpp`, BMI2 isolation (`patterns_bmi2.cpp`), scalar `flips()` pragmas, 64-byte seqlocked TT, scoped thread pool and YBWC |
| [`stello_ffi.hpp`](../Stello.Cpp/src/ffi/stello_ffi.hpp), [`stello_ffi.cpp`](../Stello.Cpp/src/ffi/stello_ffi.cpp) | The 14 `extern "C"` ABI-3 entry points, layout `static_assert`s and exception-safe boundary |
| [`src/wasm/`](../Stello.Cpp/src/wasm) (`protocol.cpp`, `threads.cpp`, `wasm_exports.cpp`, `stello_wasm.js`) | JSON protocol, Emscripten pthread worker pool glue and standalone ES module exports |
| [`build_ffi.cmd`](../Stello.Cpp/cmake/build_ffi.cmd), [`build_wasm.cmd`](../Stello.Cpp/cmake/build_wasm.cmd), [`build_wasm.sh`](../Stello.Cpp/cmake/build_wasm.sh), [`embed_eval.cmake`](../Stello.Cpp/cmake/embed_eval.cmake) | Cross-platform Clang FFI, Emscripten WASM and zero-dependency weight-embedding scripts |
| [`Stello.Engine.csproj`](../Stello.Net/Stello.Engine/Stello.Engine.csproj), [`Stello.Web.csproj`](../Stello.Net/Stello.Web/Stello.Web.csproj), [`azure-static-web-apps.yml`](../.github/workflows/azure-static-web-apps.yml) | MSBuild targets `BuildStelloNative`, `BuildStelloWasm`, `BuildStelloWasmThreads`, and CI `setup-emsdk@v15` deploy |
| [`StelloLibraryMatchPlayer.cs`](../Stello.Net/tools/Stello.Benchmark/StelloLibraryMatchPlayer.cs), [`MatchCommands.cs`](../Stello.Net/tools/Stello.Benchmark/MatchCommands.cs), [`BenchSearcher.cs`](../Stello.Net/tools/Stello.Benchmark/BenchSearcher.cs) | `engine=stello-v3` preservation alongside `stello` and `stello-lib` |

## Tests

- **C++ (`ctest` across all 5 presets plus `STELLO_ABORT_EXCEPTIONS=OFF`):** `43/43` unit tests in [parity_tests.cpp](../Stello.Cpp/tests/parity_tests.cpp), [search_tests.cpp](../Stello.Cpp/tests/search_tests.cpp), [threads_tests.cpp](../Stello.Cpp/tests/threads_tests.cpp), [ffi_tests.cpp](../Stello.Cpp/tests/ffi_tests.cpp) and [wasm_protocol_tests.cpp](../Stello.Cpp/tests/wasm_protocol_tests.cpp) cover bitboards/perft (`6,214` positions), pattern weights, all `105` 3.1 signatures, 8-thread 1M-round seqlock races, Move Now / cancel latency, all C ABI functions, and WASM JSON parsing/formatting.
- **.NET (`dotnet test -c Release`):** [`NativeEngineTests.cs`](../Stello.Net/Stello.Engine.Tests/NativeEngineTests.cs) (`40000` / ABI 3), [`StelloV3BaselineTests.cs`](../Stello.Net/Stello.Engine.Tests/StelloV3BaselineTests.cs) (all `105` strict signatures), and the rest of `Stello.Engine.Tests` (**497** passed), `Egaroucid.Engine.Tests` (**97** passed, 8 skipped for optional tagged 3.0), and `Stello.Net.Tests` (**116** passed, including [`ViewModelTests.cs`](../Stello.Net/Stello.Net.Tests/ViewModelTests.cs) `AboutInfo` `"Stello Version 4.0"`).
- **WebAssembly (`node --test`):** **15/15** tests in [engine.test.mjs](../Stello.Rust/stello-wasm/tests/engine.test.mjs) (`/^4\.\d+\.\d+$/`, node parity, callbacks, limits, Move Now / cancel) and [engine-mt.test.mjs](../Stello.Rust/stello-wasm/tests/engine-mt.test.mjs) (4-thread FFO #40–44 exact solves and < 30 ms stop latency).

## Further reading

- [Stello 4.0 C++ implementation.md](../Plans/Stello%204.0%20C++%20implementation.md): the two-part plan and Gate P4 decision.
- [Stello 4.0 C++ findings.md](../Plans/Stello%204.0%20C++%20findings.md): complete benchmark tables, `llvm-objdump` counts, FFI/WASM comparisons and Gate P0–P7 records.
- [Chapter 25](25-phases-0-4-the-engine-in-rust.md), [chapter 26](26-phases-5-9-threads.md) and [chapter 27](27-stello-3-1-fixes-and-the-desktop-app.md): the Rust 3.0 rewrite, threading design and 3.1 baseline that `Stello.Cpp` ports 1-to-1.
- [References](30-references.md#28-stello-40-the-engine-in-c): C++20 `<bit>`, Clang, Emscripten and WebAssembly exception handling.

---

Previous: [27 Stello 3.1: fixes and the desktop app](27-stello-3-1-fixes-and-the-desktop-app.md) · Next: [29 Glossary](29-glossary.md)
