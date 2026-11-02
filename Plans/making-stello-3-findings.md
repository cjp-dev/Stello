# Findings from making Stello 3.0

What the work on Stello 3.0 teaches us, written down as we go: the decisions, the measurements, what worked and what did not, and the ideas for later.

- **The plan:** [making-stello-3-plan.md](making-stello-3-plan.md). This file is updated after every phase.
- **Where it starts:** Stello 2.0 as released on 2 November 2026 ([docs chapter 24](../docs/24-results-and-what-comes-next.md)), and the C++/C# speed comparison ([cpp-vs-csharp-speed.md](cpp-vs-csharp-speed.md)).

## Status

| Phase | State | What it showed |
|---|---|---|
| 0 Set-up and the frozen Stello 2.0 | done (2026-11-02) | Stello.Engine.V2 searches node for node like 2.0; the Rust workspace builds for Linux, Windows and WASM, and .NET and Node call it |
| 1 The core in Rust | not started | |
| 2 Desktop and tools on Rust | not started | |
| 3 The web app on Rust | not started | |
| 4 Release (3.0 preview) | not started | |
| 5 Threads and a shared hash table | not started | |
| 6 Lazy SMP | not started | |
| 7 YBWC | not started | |
| 8 Threads in the browser | not started | |
| 9 Threads setting and the 3.0 release | not started | |

## Baseline (Stello 2.0, C#, 1 thread)

| What | Value | Source |
|---|---|---|
| FFO #40–49, exact | 46.8 s, 1.10 G nodes | [v2-final-solve-bench-40-49.json](benchmark-results/v2-final-solve-bench-40-49.json) |
| Midgame speed in matches, 500 ms | about 4.2–4.5 M nodes/s | [docs chapter 24](../docs/24-results-and-what-comes-next.md) |
| Browser, 22 empty squares | solved in 0.49 s (live check) | [improve-stello-strength-findings.md](improve-stello-strength-findings.md) |
| Book recalculation, 60 s per position, 8 workers | about 14 h (Ryzen 7 7800X3D) | [docs chapter 23](../docs/23-phase-5-the-search.md) |

## Phase 0: set-up and the frozen Stello 2.0 (2026-11-02)

### Toolchain
Installed in the sandbox (Ubuntu 26.04). The PATH is set in `/etc/sandbox-persistent.sh` via `~/.cargo/env`.

| Tool | Version | Used for |
|---|---|---|
| rustup, stable Rust | rustc 1.99.1, cargo 1.99.1 | everything |
| Targets | `x86_64-unknown-linux-gnu`, `x86_64-pc-windows-gnu`, `wasm32-unknown-unknown` | tools and tests, WPF, web |
| MinGW-w64 (apt `gcc-mingw-w64-x86-64`) | GCC 13 (win32 threads) | linker for the Windows DLL (`Stello.Rust/.cargo/config.toml`) |
| wasm-bindgen-cli | 0.2.130 (`cargo install --locked`) | JS glue for the WASM package; the crate is pinned to `=0.2.130` |
| Node | 22.22.1 (already installed) | quick WASM checks |

### Stello.Engine.V2
- **Contents:** a copy of `SearchEngine` (with `Endgame/`), `Search/`, `Evaluation/` and `ComputerPlayer`, in the namespaces `Stello.Engine.V2.*`. It has its own embedded `stello-eval.bin` (resource `Stello.Engine.V2.stello-eval.bin`).
- **Shared with `Stello.Engine`:** `Board`, `Move`, the limits, the results and the opening book. `Stello.Engine` gives it `InternalsVisibleTo`.
- **Match and benchmark keys:** `engine=stello-v2`, `--engine stello-v2` (`solve-bench`, `mid-bench`, with `--eval`), and `stello-v2` in `mid-quality --engines`. `V2MatchPlayer` plays from the current book.
- **Gate:** identical to today's engine. It visited the same nodes and found the same moves and scores on all 100 `mid-bench` positions at depth 10 (3,719,468 nodes) and on FFO #40–49 (1.10 G nodes).
- **Tests:** `StelloV2Tests`, with 105 recorded signatures (`Data/stello-v2-signatures.txt`): the 100 midgame positions at depth 10 and FFO #40–44 solved, each with move, score, kind, depth and nodes.
  - The frozen engine must keep matching them.
  - So must the engine being developed: once `Stello.Engine.SearchEngine` calls Rust, this is the parity test.
  - Engine tests: 469 (259 + 2 × 105), all green.

### Stello.Rust
- **Location:** a Cargo workspace (edition 2024, GPL-3.0-only, version 3.0.0) at the repository root.
- **Crates:** `stello-core` (the engine, empty so far), `stello-ffi` (the `cdylib` `stello_engine`), `stello-wasm` (wasm-bindgen) and `stello-cli` (the binary `stello`).
- **Release profile:** fat LTO, one codegen unit, `panic = "abort"`, so no panic unwinds into .NET or JavaScript.
- **Builds:**
  - Linux `libstello_engine.so`: 333 KB;
  - Windows `stello_engine.dll`: 324 KB. It imports only system DLLs (KERNEL32, msvcrt, ntdll, api-ms-win-core-synch), so no MinGW runtime has to ship;
  - `stello_wasm.wasm`: 36 KB, plus the wasm-bindgen package (`--target web`).
- **Checks:**
  - a .NET program with `[LibraryImport("stello_engine")]` calls `stello_version()` and gets 30000;
  - Node loads the WASM package and gets "3.0.0".
- **Not committed:** `target/` and `pkg/` (`.gitignore`).

### Noted
- The host's Visual Studio and the sandbox share the `obj/` folders. After a restore on Windows, the Linux build failed with "Unable to find fallback package folder 'C:\Program Files (x86)\…\NuGetPackages'". A `dotnet restore` here fixes it, and the reverse may happen on Windows.
