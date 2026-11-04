# Making Stello 3.0

This plan follows [improve-stello-strength-plan.md](improve-stello-strength-plan.md) (Stello 2.0, closed on 2 November 2026). It gets a Status section that is updated after each phase. Its findings document, [making-stello-3-findings.md](making-stello-3-findings.md), is updated after every phase.

## Goal

**Part 1:** Stello 2.0's engine is rewritten in Rust, and the apps and tools use it. The goal is speed in three places:
- the desktop app;
- the web app;
- the tools that search: the book recalculation, the benchmarks and the trainer's fits.

**Part 2:** parallel search, Lazy SMP and YBWC, inspired by Egaroucid's implementation.

### Why
- **Rust's speed:** the C++/C# comparison ([cpp-vs-csharp-speed.md](cpp-vs-csharp-speed.md)) measured the same Egaroucid search in both languages:
  - native C++ is 1.5 times faster than C#, and 2.3 times with AVX2;
  - in WebAssembly, C++ is 1.9–2.7 times faster than C# AOT.

  Rust compiles through LLVM like Clang, so it should reach C++'s speed.
- **What speed is worth:** the time-odds matches put twice the time at +84 Elo at 100 ms and +38 at 500 ms ([docs chapter 23](../docs/23-phase-5-the-search.md)).
- **Threads multiply with speed:** the desktop machines have 16–24 threads, but the engine uses one.
- **Where 2.0 stands:** it beats the Egaroucid port at 100 and 500 ms, but loses at 2 s, where depth counts most ([docs chapter 24](../docs/24-results-and-what-comes-next.md)).

## Decisions (user, 2 November 2026)

| Question | Decision |
|---|---|
| Toolchain | Install rustup with stable Rust, the `wasm32-unknown-unknown` target and `wasm-bindgen-cli` now. Nightly comes only in Part 2, for threads in the browser. This is an exception to the ".NET only" rule, for this plan. |
| Web engine | Rust compiled to WebAssembly with wasm-bindgen, running in a small JavaScript Web Worker. The Blazor UI keeps its engine host. The .NET worker goes. |
| Boundary | The search core moves to Rust: board and rules, evaluation, midgame search, endgame solver, hash table and time control. A thin C# `SearchEngine` wrapper keeps today's API, so the book code, game records, apps and tools need no changes. The C# engine is frozen as **Stello 2.0** (`Stello.Engine.V2`) for matches, as Stello 1 was. |
| Releases | The apps get the Rust engine after Part 1 and the threaded one after Part 2. About shows 3.0 once Part 1 is live. 3.0 is final after Part 2. |
| Windows DLL | Built on the user's PC by Cargo itself, called from MSBuild like in the sandbox (the user installed rustup on Windows, 2026-11-02; the default MSVC toolchain needs Visual Studio's C++ build tools). The sandbox still cross-compiles `x86_64-pc-windows-gnu` with MinGW as a check. Each OS has its own Cargo target folder, because the repository folder is shared. Nothing binary is committed. |
| Part 1 gate | Node-for-node identical to the C# 2.0 engine: the same moves, scores and node counts. SIMD speed-ups must keep the node counts. |
| Threads in the apps | A "Threads" setting in both apps. The default is the number of cores minus one on the desktop, and `navigator.hardwareConcurrency` in the browser. Matches and tools get a `threads=` key. |

## The engines

| Name | Project | Match side | What it is |
|---|---|---|---|
| Stello 1 | `Stello.Engine.V1` | `engine=stello-v1` | The engine before 2.0, frozen |
| Stello 2.0 | `Stello.Engine.V2` (new) | `engine=stello-v2` | The C# engine as released on 2 November 2026, frozen |
| Stello 3.0 | `Stello.Engine` (C# API) and `Stello.Rust` (Rust core) | `engine=stello` | The engine being built |
| Egaroucid | `Egaroucid.Engine` | `engine=egaroucid` | The C# port of Egaroucid 7.9, the reference |

## Investigation: what uses the engine

All engine users reach the search through `SearchEngine`: directly, or through `ComputerPlayer`, `BookSearch`, `BookRecalculator`, `BookLearner` or `BookMatch`. A C# wrapper with the same API therefore moves all of them to Rust at once. This table is the result of reading the project references and the code on 2 November 2026; phase 0 confirms it.

| User | How it uses the engine | Change needed | Speed gain |
|---|---|---|---|
| **Stello.Net** (WPF) | `LocalEngineHost` → `ComputerPlayer`, and `BookLearner` (self-play, Evaluate Book) | Ship the native DLL next to the exe | Every computer move, and book learning |
| **Stello.App** (shared view models) | Through `IEngineHost` | None | — |
| **Stello.Web** | `EngineWorker` (.NET in a Web Worker): `ComputerPlayer` with the book | New JS worker with Rust WASM. Book lookup moves to `WebEngineHost` on the UI side; Stop and Move Now keep the shared-memory signal | Every computer move in the browser |
| **Stello.BookTool** | `recalc` (`BookRecalculator`, 8 workers × 60 s per position, 12–16 h), `match` (`BookMatch`) | None | **The largest**: a faster engine reaches more depth in the same 60 s, or finishes sooner with `--depth` |
| **Stello.Benchmark** | `match`, `solve-bench`, `mid-bench`, `mid-quality`, `make-positions`, `make-starts` (`--engine`, side keys) | Add `engine=stello-v2` / `--engine stello-v2` for the frozen C# engine | Faster matches |
| **Stello.Trainer** | `mpc` and `mpc-end` search with the engine (hours); `train`, `report` and `convert` use only `EvalWeights` | None for `mpc`/`mpc-end`. `train` and `report` stay C#; the Rust engine reads the same `stello-eval.bin` | `mpc`/`mpc-end` |
| **Stello.Engine.Tests** | Search, endgame, book and learner tests | They run against the wrapper, so against Rust. New parity tests compare it with `Stello.Engine.V2` | — |
| **Egaroucid.Engine.Tests**, **Stello.Engine.V1** | `Board` and other shared types | None (the shared types stay C#) | — |

What stays in C#: `Board`, `Move`, `Square`, `Game`, `SearchLimits`, `SearchResult`, the opening book and its formats, `BookRecalculator`, `BookLearner`, `BookMatch`, `ComputerPlayer`, and the apps.

What moves to Rust is about 3,600 lines of C#:
- `SearchEngine.cs` (809 lines);
- `Endgame/` (about 1,470);
- `Evaluation/` (about 610);
- `Search/` (about 310);
- `Bitboards` and `Board`'s move generation (about 250).

## Part 1: the engine in Rust

### Phase 0: set-up and the frozen Stello 2.0
- **Branch** `feature/stello-3`, with this plan and the findings document.
- **Toolchain:**
  - rustup with stable Rust;
  - the `wasm32-unknown-unknown` target and `wasm-bindgen-cli`;
  - the `x86_64-pc-windows-gnu` target with MinGW.

  The versions are recorded in the findings.
- **`Stello.Net/Stello.Engine.V2`:** a copy of today's search, evaluation and endgame code, in the namespace `Stello.Engine.V2`. It has a `V2MatchPlayer` and the match keys `engine=stello-v2` / `--engine stello-v2`.
- **`Stello.Rust/`, a Cargo workspace (GPL-3.0-only):**
  - `stello-core`: the engine, plain Rust;
  - `stello-ffi`: a C ABI library (`cdylib`) for .NET;
  - `stello-wasm`: the wasm-bindgen package for the browser;
  - `stello-cli`: a small command-line tool for parity and speed runs without .NET.
- **Gate P0:**
  - V2 is identical to today's engine: the same moves, scores and nodes on `mid-bench` and on FFO #40–49;
  - all tests pass;
  - the toolchain builds an empty `cdylib` for Linux and Windows, and a WASM package.

### Phase 1: the core in Rust, parity first
Ported in dependency order. Each step gets a parity check against C# before the next one starts.

1. **Bitboards, move generation and flips.** Gate: perft 1–8 equals C#, and random positions give the same moves.
2. **Evaluation:**
   - read `stello-eval.bin` (gzip, version 2);
   - the same index extraction and folding.

   Gate: bit-for-bit equal evaluations on the 1.04 M test positions and on random positions.
3. **Midgame search:**
   - PVS, Multi-ProbCut with the same error table;
   - move ordering (the response table, evaluation ordering from look 4), ETC;
   - the two-entry transposition table with the same hash function and replacement;
   - iterative deepening, time control (the 2/3 rule), and the switches of `SearchOptions` (another evaluation file, `Selective`, `ProbCutPercent`, `SoftTimePercent`).
4. **Endgame solver:** `EndWindow`, `EndDeep`, `EndLocal`, `EndSimple`, `EndFast`, `Last4`–`Last1`, `SolveShallow`, the stable-disc tables, the local table and the two passes.
5. **Stopping and progress:** abort checks at the same node counts, heartbeats every 100 ms, Move Now and cancellation through atomic flags.

- **Parity tool:** `stello-cli` and `Stello.Benchmark` write the same JSON for `mid-bench` and `solve-bench`. `--compare` checks nodes, moves and scores entry by entry, as for Stello 1 and the Egaroucid port.
- **Speed work only after parity,** and only where it keeps the nodes:
  - portable 64-bit code first (WASM has no BMI2/AVX2);
  - on the desktop, AVX2/BMI2 paths chosen at run time (`is_x86_feature_detected!`), with the portable path as the fallback.
- **Gate P1:**
  - identical nodes, moves and scores on `mid-bench` at depth 10 and 12, on FFO #40–49, and on 1,000 random positions at depth 8;
  - the evaluation is equal on the whole test split;
  - speed is measured: Rust native against C# native, single thread.

### Phase 2: desktop and tools on Rust
- **`Stello.Engine.SearchEngine` becomes a wrapper:**
  - `[LibraryImport]` calls into `stello-ffi`, with the engine as an opaque handle (hash bits, options, evaluation);
  - `Search(board, player, limits, progress, cancellationToken, moveNowToken, onlyMoves)` keeps its signature;
  - progress comes through a function-pointer callback;
  - cancellation and Move Now set atomic flags in the engine;
  - `ClearHash`, `HashBits` and `Options` stay.
- **Native library packaging:**
  - `runtimes/linux-x64/native` for the tools and tests here, and `runtimes/win-x64/native` for the WPF app;
  - built by a Cargo step that MSBuild calls when the Rust sources change. The built libraries are not committed.
- **No changes needed** in BookTool, Benchmark or Trainer apart from `engine=stello-v2`. This is checked by running each command once.
- **Gate P2:**
  - all engine tests pass on the wrapper, and the parity run still holds through it;
  - `solve-bench` and `mid-bench` are faster than C# 2.0. The target is at least 1.5 times; the result is recorded either way;
  - a 1,180-game match of 3.0 against 2.0 at 500 ms scores above 50 %, so the speed pays off in play;
  - the book recalculation is timed on a 200-leaf sample with `--depth`: identical values, and the time against C#;
  - WPF builds with the Windows DLL. The user runs the desktop app on their PC.

### Phase 3: the web app on Rust
- **`stello-wasm`** exports `init(eval, hashBits)`, `search(request)` and a progress callback. It reads the stop and Move Now signal from a `SharedArrayBuffer`, as the .NET worker does today.
- **`engine-worker.js`** loads the Rust WASM instead of the .NET runtime.
- **`WebEngineHost`:**
  - looks up the book itself, with `BookTracker`, and only sends searches to the worker;
  - keeps the same protocol (`EngineProtocol`) and the 300 ms fallback to terminate and restart;
  - the worker no longer needs the book file.
- **The deploy workflow** installs Rust, the WASM target and wasm-bindgen before the .NET build.
- **Gate P3:**
  - the Release build in Chromium (Playwright) plays, Stop and Move Now answer within 100 ms, and a 22-empty-square position is solved;
  - the browser's speed against today's .NET AOT worker on the same positions: the target is at least 1.9 times;
  - the worker's start-up time is noted;
  - the live check after the deploy.

### Phase 4: release the Rust engine (3.0 preview)
- About shows "Stello Version 3.0". The docs' Part 1 is updated where the engine is described (chapters 01, 03–09, 14), and a Part 3, "From 2.0 to 3.0", gets its first chapter.
- Fast-forward main, deploy, live check.

## Part 2: parallel search

Modelled on Egaroucid ([Egaroucid-main/src/engine](../Egaroucid-main/src/engine)):
- **`ai.hpp`:** a Lazy-SMP-like set of helper searches while the main iterative deepening runs at depths up to 10. Helper *i* searches deeper (main depth + ctz(i + 1)) or with more selectivity, and fills the shared hash table.
- **`ybwc.hpp`:** Young Brothers Wait Concept. After the first move of a node has been searched, the remaining moves may be split to idle threads.
- **`thread_pool.hpp` / `thread_pool_with_idling_manager.hpp`:** a fixed pool, with tasks only pushed while threads are idle.

The Egaroucid port did not port this; its YBWC phase was postponed.

### Phase 5: threads and a shared hash table (desktop and tools)
- **A thread pool in `stello-core`:**
  - `std::thread` and a work queue, started once per engine;
  - the size comes from `SearchLimits`, a new `Threads` field, defaulting to 1.
- **A thread-safe transposition table:**
  - entries written so that a torn read cannot be mistaken for a valid one (packed 64-bit words with a check, or a per-slot lock as in Egaroucid);
  - it is measured for speed on 1 thread.
- **With 1 thread the engine stays node-for-node identical to Part 1,** so the parity checks still hold.
- **Gate P5:**
  - threads = 1 is identical;
  - `solve-bench` and `mid-bench` with N threads give the right results;
  - the overhead on 1 thread is at most 3 %.

### Phase 6: Lazy SMP
- Helper searches as in Egaroucid's `ai.hpp`:
  - at main depths up to 10, helpers search the root at deeper depths or higher selectivity levels into the shared table;
  - they are stopped when the main iteration ends;
  - their nodes are counted.
- In the endgame solve, helpers only when it measurably helps (Egaroucid uses none in the last iteration).
- **Gate P6:**
  - matches at 500 ms, 1,180 games each, of 4 and of 8 threads against 1 thread: lower bound above 50 %;
  - speed-up on `mid-bench` and the time-to-depth are recorded.

### Phase 7: YBWC
- **Splitting:** after the first child of a node is searched, the remaining children go to idle threads, above a minimum depth and only where it pays (Egaroucid's `ybwc.hpp` rules).
  - In the midgame: PVS nodes and null-window nodes.
  - In the endgame: `EndWindow`/`EndDeep` above the local-table depth.
- **Cutoffs** stop the siblings through a shared "searching" flag per split node, as Egaroucid does.
- **Lazy SMP and YBWC together,** as in Egaroucid: YBWC in the main search, helpers at shallow main depths.
- **Gate P7:**
  - FFO #40–49 with N threads is correct and faster: the speed-up for 4, 8 and 16 threads is recorded, against Egaroucid C++ and the port;
  - a 500 ms match of YBWC + Lazy SMP against Lazy SMP alone, at the same thread count: not worse, and better at 2 s.

### Phase 8: threads in the browser
- **Nightly Rust,** with `-Z build-std=std,panic_abort` and `+atomics,+bulk-memory`, and a pool of Web Workers that share the WASM memory (wasm-bindgen-rayon or our own pool).
- **The site already sends COOP/COEP** (`staticwebapp.config.json`), as Checkers' WASM threads needed.
- **Fallback:** if `SharedArrayBuffer` or threads are missing, the single-threaded WASM build.
- **Gate P8:** in the browser, N threads against 1 on the same positions (speed and results); the live check with threads.

### Phase 9: the Threads setting and the 3.0 release
- **Apps:**
  - a "Threads" setting in WPF and web, defaulting to the cores minus one on the desktop and `navigator.hardwareConcurrency` in the browser;
  - stored with the other settings;
  - shown in the analysis as nodes per second over all threads.
- **Tools:**
  - a `threads=` side key and `--threads` options;
  - ~~the book recalculation's `--workers` × `--threads` are measured to find the best split on a given machine.~~ User decision (2026-11-03): the recalculation keeps 1 thread per worker and has no `--threads`, because parallel positions scale better than threads in one search; threads stay in the matches and benchmarks (`Stello.Benchmark`, `Stello.BookTool match`).
- **Final measurements,** as for 2.0:
  - against 2.0 and against Egaroucid, at 100 ms, 500 ms and 2 s, with 1 thread and with N threads;
  - FFO #40–49.
- **Release:** docs (Part 1 updated, Part 3 chapters), About 3.0, fast-forward main, deploy, live check.

## After the plan (user, 2026-11-02)
- **Align the WPF GUI with the web apps.** This comes after phase 9, once the plan is finished.

## Working rules (as for 2.0)
- **Git:**
  - one commit per step, with the identity `-c user.name=cjp-dev -c user.email=cjp230364@gmail.com` and the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`;
  - `git fsck --full --no-dangling` after each commit;
  - `env -u GH_TOKEN` for push and `gh`;
  - fast-forward main only at a release.
- **Never commit:** training data, built binaries (the Rust `target/`, the DLL and `.so` files, `.wasm` packages), or `Egaroucid-main`.
- **Matches:** 1,180 games from `starts.txt`, with no other heavy work on the machine while they run.
- **Findings:** updated after every phase, with the measurements, what worked and what did not.
- **Docs:** updated at each release. Part 3, "From 2.0 to 3.0", gets one chapter per part.

## Verification summary
| Gate | Check |
|---|---|
| P0 | Stello 2.0 frozen and identical; the toolchain builds Linux, Windows and WASM |
| P1 | Rust identical to C# 2.0 (perft, evaluation, mid-bench, FFO, random positions); speed measured |
| P2 | Tests pass on the wrapper; ≥ 1.5× native speed (target); 3.0 against 2.0 > 50 % at 500 ms; book recalculation identical and faster |
| P3 | Browser plays, stops and solves; ≥ 1.9× browser speed (target); live check |
| P5 | 1 thread identical; N threads correct; ≤ 3 % overhead |
| P6 | Lazy SMP with 4 and 8 threads beats 1 thread (lower bound > 50 %) |
| P7 | YBWC: FFO correct and faster; with Lazy SMP not worse than Lazy SMP alone |
| P8 | Browser threads faster and correct; fallback works |
| P9 | Threads setting; final measurements; 3.0 live |

## Status
- Plan written on 2 November 2026.
- **Phase 0: done (2026-11-02).** Toolchain installed (Rust 1.99.1, wasm-bindgen 0.2.130, MinGW 13). `Stello.Engine.V2` is identical to 2.0 on mid-bench depth 10 and FFO #40–49, with 105 recorded signatures tested for both engines. `Stello.Rust` builds the Linux library, the Windows DLL and the WASM package, and .NET and Node call them. Details in the findings.
- **Phase 1: done (2026-11-02).** The Rust core (board, evaluation, midgame search, endgame solver) is node-for-node identical to C# 2.0 on mid-bench depth 10 and 12, FFO #40–49, 1,000 random midgame and 1,800 random endgame positions, and the hand-over to the solver. The evaluation is equal on the whole test split. Speed on one thread: 1.27–1.37× without any speed work.
- **Phase 2: done (2026-11-02, 8fd3f98 wrapper, 8220705 speed pass).** `Stello.Engine.SearchEngine` calls the Rust core through a C ABI with the same API; MSBuild builds it with Cargo on Linux and Windows. Identical to 2.0 through the wrapper on every parity run and a whole-book recalculation. After a speed pass with identical nodes it is 2.8–3.1× faster in the midgame and 1.75× on FFO #40–49, and 3.0 beats 2.0 at 500 ms with 59.6 % (+68 Elo, 1,180 games). The user tested WPF on Windows (VS Code, `dotnet run`): it builds the MSVC DLL with Cargo and plays.
- **Phase 3: done (2026-11-02, 458c50a and the follow-up).** The web app searches with the Rust engine in WebAssembly, in a plain JS worker; the page does the book move and book learning. In Chromium it is identical to 2.0, 2.30–2.40× faster in the midgame and 1.59× in the solver (1.86× overall, against the 1.9× target; a profile shows WebAssembly is within 25 % of native, so the gap is in the native solver). Move Now and cancel answer in under 1 ms in the engine, worker start-up is 92 ms instead of 680 ms, and the first visit is 6.84 MB instead of 7.91 MB (the C# evaluation is no longer embedded in Stello.Engine). The deploy workflow installs Rust and wasm-bindgen; it first runs on GitHub at the phase 4 release.
- **Phase 4: done (2026-11-02).** The 3.0 preview is live: About shows 3.0, the docs describe the Rust engine and Part 3 begins with chapter 25. main = c9aa054, and the deploy (with the new Rust steps) succeeded. The live site plays, stops (Move Now 28 ms) and solves (22 empty squares in 0.26 s) on the Rust WebAssembly engine. **Part 1 is finished;** Part 2 (threads) starts with phase 5.
- **Phase 5: done (2026-11-02).** A thread pool (`search/pool.rs`) and a seqlocked shared hash table (64-byte slots, so 64 MB at the default size instead of 48), and `Threads` from C# `SearchLimits` through FFI ABI 2 to Rust, the web protocol and the tools (`--threads`, `threads=`). threads = 1 is identical to 2.0 on every parity run with at most +2.2 % overhead; 2–16 threads give exact solves. No speed-up yet: that is phase 6. Details in the findings.
- **Phase 6: done (2026-11-02).** Lazy SMP: helpers run their own iterative deepening 1 + ctz(i+1) plies ahead at all midgame depths (Egaroucid's limit of depth 10 gave no gain without YBWC), and up to 3 helpers help each solve pass. 4 threads beat 1 with 55.3 % (53.1–57.4) and 8 threads with 53.0 % (50.9–55.0) at 500 ms, 1,180 games each; time to depth 1.53× (4) and 1.66–1.76× (8). threads = 1 stays identical. Details in the findings.
- **Phase 7: done (2026-11-03).** YBWC after Egaroucid's ybwc.hpp (midgame from depth 6, endgame from 16 empties) plus Lazy SMP on half the workers at all depths (Egaroucid's shallow-only helpers were slower here). FFO #40–49 1.98×/3.29×/3.97× faster with 4/8/16 threads, all correct; depth 16 2.09× with 8 threads. 8 threads YBWC + Lazy SMP against Lazy SMP alone: 54.4 % at 500 ms (1,180 games) and 54.7 % at 2 s (600 games). threads = 1 stays identical. Details in the findings.
- **Phase 8: done (2026-11-03).** A threaded WebAssembly package (`wwwroot/engine-mt`, nightly-2026-11-02 with build-std, shared memory) whose threads are Web Workers through the engine's Spawner hook; loaded only when more than 1 thread is asked for and the page is cross-origin isolated, else the single-threaded package. In Chromium with 8 threads FFO #40–44 is 3.5× and the midgame 1.93× faster, all exact; Move Now ≤ 7 ms. For testing until phase 9: `?threads=N`. The deploy workflow installs the nightly; it first runs at the release. Details in the findings.
- **Phase 9: part A done (2026-11-03).** A Threads setting in WPF and web (Automatic = cores − 1 on the desktop, `hardwareConcurrency` in the browser; the web worker restarts between 1 and more threads). The recalculation stays at 1 thread per worker; `Stello.BookTool match --threads` added. The final matches (part B) wait for the user's go.
- **Phase 9: part B done (2026-11-04).** The final matches (13 runs, each on the user's go): against 2.0, 1 thread +99/+78/+83 Elo and 8 threads +142/+121/+90 at 100 ms/500 ms/2 s; against the Egaroucid port, 1 thread +273/+109/−30 and 8 threads +237/+115/−1. FFO #40–49: 25.8 s (1 thread), 7.81 s (8), 6.56 s (24). Docs done (2026-11-04): chapter 26 Phases 5–9, Part 1 updated, glossary/references now 27/28. Left: the user's WPF check, the release.
