# Handoff: Stello 3.1 development (snapshot 2026-11-07)

For the next session. Repository: `C:\Udvikling\Privat\Spil\Stello` on the user's Windows machine (GitHub `cjp-dev/Stello`). Branch **`feature/stello-3.1`**, phase 2 items 5 and 6 committed; the user pushes (section 6, Git).

## 1. The objective
Stello is an Othello program: a Rust engine with C# apps, WPF on the desktop and Blazor WebAssembly on the web. 3.0 was released on 2026-11-04 (main `baa44e5`, git tag `v3.0`).

The plan is **[Plans/making-stello-3-1-plan.md](../making-stello-3-1-plan.md)**, with the findings in **[Plans/making-stello-3-1-findings.md](../making-stello-3-1-findings.md)**. Read both first; the findings are updated after every item.
- **Part 1, Stello 3.1:**
  - phase 0: set-up;
  - phase 1: real bugs;
  - phase 2: the parity quirks kept from C#/C++, investigated one by one;
  - phase 3: the apps (full WPF alignment with the web app, a Computer vs Computer mode);
  - phase 4: tools and verification gaps (match save/resume, the Egaroucid port's time overruns behind a side key, FFO #50–59, old Plans links);
  - phase 5: the 3.1 release.
- **Part 2, Stello 3.2 (phases 6–12):** speed, threads, search, endgame, evaluation, the opening book, and the 3.2 release.

## 2. Completed
- **Phase 0** (`e0a6cf7`):
  - the 3.0 one-thread reference JSONs in `Plans/benchmark-results/v3.0-ref/`;
  - `StelloV3BaselineTests` with `Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt` (105 positions);
  - repeatable `opt=<key>=<value>` side switches in `Stello.Benchmark match`.
- **Build fix** (`76e9e14`): a root `Directory.Build.props` puts the intermediate files in `obj/$(OS)/` (`obj/Unix`, `obj/Windows_NT`). Windows and the sandbox no longer break each other's NuGet restore.
- **Phase 1, done** (`17a27ff`):
  1. an aborted solve is no longer reported as Exact (`fd1d050`, `59be3c5`);
  2. the 250 ms natural delay is no longer charged to the clock (`91e132c`);
  3. Back/Forward restore both clocks per ply (`afa9b1d`);
  4. the endgame soft limit counts from the search's start (`b33c942`; gate matches at 1 and 5 min per game; switch removed in `ae8dfc9`);
  5. web worker robustness and late progress reports (`9be8774`; the review fixed a restart loop: `e.preventDefault()` in `engine-worker.js`);
  6. doc errors (`0d8d5eb`).

  The gate: item 1 was checked in the web app against live 3.0, and item 2 by the user in WPF.
- **Phase 2, items 1–6:**

  | Item | Commit | Result |
  |---|---|---|
  | 1. An upper bound equal to alpha cuts off | `7d01e75` | kept; 500 ms match 50.2 % |
  | 2. Upper-bound hash replies in the response table | `bbe2a60` | harmless; left as is |
  | 3a. TT aging (entries with more empties than the root count as empty) | `7a96a59` | kept; 500 ms 50.6 %, 2 s 47.5 % (starts 1–300), confirmation 2 s 51.6 % (starts 301–590) |
  | 3b. Keeping tighter bounds | — | dropped: no gain |
  | 4. `end_deep` keeps the move of its best ETC fail-low | `0eb00cc` | kept; 500 ms 50.3 %; FFO #43/#44 node counts changed |
  | 5. Moves decided by the local table leave the sort (`undone_first`) | `db01cf2` | kept; 500 ms 49.6 % (48.0–51.1); FFO #40–44 node counts changed; speed unchanged |
  | 6. Response ordering: the hash move is swapped to the front, and the loop stops one move early | — | harmless, no fix: the swap only changes ties; a rotate searched +0.02–0.05 % more midgame nodes; a single last move needs no sort |

## 3. In progress: phase 2, items 7–9, then gate P2
The item texts are in the plan, under "Phase 2". Its file:line references may be stale, so search for the code.
- **7:** in `search_root` (`engine.rs`), every root move gets a full window up to depth 3 (`i == 0 || look <= 2`).
- **8:** the Multi-ProbCut shallow search reads and writes the hash table at the same ply (hash heights).
- **9:** the counting quirks, which only affect statistics:
  - two abort-check schemes share one node counter;
  - evaluations are counted at final scores in some layers only;
  - `ordering_value` counts nodes without an abort check;
  - `last4`–`last1` count each pass as a node;
  - `solve_shallow`/`solve_last2` can skip abort checks.
- **Gate P2:** every item has a verdict. Then all kept fixes together play `v3.0` in a 1,180-game match at 500 ms and a 600-game match at 2 s, not worse. Comparing with 3.0 needs either a frozen 3.0 build or the switches; the plan expects the latter, but they are removed after each item. Decide with the user how to run it, for example a second native library built from tag `v3.0`, or reverse switches.

## 4. How we work (user decisions)
- **Who does what:** an agent on the user's Windows machine implements each item. This session reviews it: the diff, the tests, the baseline, the findings, plus its own spot checks. It fixes review issues and **commits**. The user is the one that does the Git **push**. The session is not allowed to do a Git **Push**. If its needed it have to ask the user. The user reports back after each item. When a item is reviewed and comitted, finish with your rewiew and a commandline showing the commando for the final push.
- **Matches only on the user's go:** give a numbered list with time estimates (see memory `feedback-long-matches-on-go`). At 12 workers on 1 thread:
  - 500 ms, 1,180 games: about 22 min;
  - 2 s, 600 games: about 40–45 min;
  - 1 min per game: about 3 h;
  - 5 min per game, 600 games: about 7.5 h.

  At 8 threads use 2 workers (500 ms: about 2 h). Check the load first (section 6), and run nothing heavy during a match. The match tool **cannot resume**: a power cut means restarting.
- **Fixes are kept unless they play worse** (upper bound above 50 % at 500 ms; confirm at 2 s if it matters). A new strategy, not a fix, must show a gain. If a significant loss shows no mechanism in the statistics, confirm it on the disjoint start positions 301–590 before dropping a fix.
- **A/B switches:** each change is temporarily a `SearchOptions` switch through Rust → FFI (`stello_engine_set_*`, an ABI bump) → C# internal `SearchOptions` → match key `opt=…`. It is **removed as soon as the match is approved**, and the ABI goes back to 3 (it is 3 now).
- **The baseline stays strict, node for node** (C# `StelloV3BaselineTests`, Rust `tests/search.rs`, Node `engine.test.mjs`/`engine-mt.test.mjs`, all reading `stello-v3-signatures.txt`). If a kept fix changes nodes, **regenerate that file with a header note**; never loosen the tests. `stello-v2-signatures.txt` pins only the frozen C# 2.0 (`StelloV2Tests`).
- **Releases:** 3.1 after Part 1, 3.2 after Part 2. Fast-forward main **only with the user's OK**; a push to main deploys the web app (Azure Static Web Apps, `docs/**` included).

## 5. Files that matter
- **Engine (Rust, `Stello.Rust/`):**
  - `stello-core/src/search/{engine,table,time,options,ordering,pool,ybwc}.rs`;
  - `stello-core/src/endgame/search.rs`;
  - the FFI is `stello-ffi/src/lib.rs` (ABI 3); the web package is `stello-wasm`.
- **C#:**
  - `Stello.Net/Stello.Engine/{SearchEngine.cs,Search/SearchOptions.cs,Native/StelloNative.cs}`;
  - the apps: `Stello.App/ViewModels/MainViewModel.cs`, `Stello.Web/wwwroot/js/engine-{worker,client,pool-worker}.js`.
- **Tools:**
  - `Stello.Net/tools/Stello.Benchmark` (`match`, `mid-bench`, `solve-bench --compare`; `Data/starts.txt` has 590 start positions);
  - `tools/Stello.BookTool` (`recalc` has no `--threads`, by user decision; `match` does).
- **Tests:** `Stello.Engine.Tests` (489), `Stello.Net.Tests` (Windows-only, 91), `Egaroucid.Engine.Tests` (42), the Rust tests (83), the Node tests (15).
- **Docs:** `docs/` chapters 01–28 (07 table, 08 solver, 09 time, 14 apps, 15 GUI, 26 threads).

## 6. Commands (Windows, PowerShell 5.1)
PowerShell 5.1 has no `&&` or `||`: chain with `;` and gate with `if ($?) { … }`. Cargo and dotnet write progress to stderr, which PowerShell reports as a `NativeCommandError` and turns `$?` false. Wrap them as `cmd /c "… 2>&1"` and check `$LASTEXITCODE`. The machine is an i7-12850HX (16 cores, 24 threads).
- **Rust:**
  ```
  cd Stello.Rust; $env:CARGO_INCREMENTAL='0'; $env:CARGO_TARGET_DIR='target/windows'
  cargo fmt --check
  cmd /c "cargo clippy --all-targets --release -- -D warnings 2>&1"
  cmd /c "cargo clippy -p stello-core -p stello-wasm --target wasm32-unknown-unknown -- -D warnings 2>&1"
  cmd /c "cargo test --release 2>&1" | Select-String 'test result|FAILED|panicked'
  ```
  `solve_signatures_match_the_baseline` stops at the first wrong line. To get all five FFO lines, use the CLI: `cmd /c "cargo build --release -p stello-cli 2>&1"`, then `.\target\windows\release\stello.exe solve-bench` (FFO #40–44).
- **.NET** (from `Stello.Net`). The build also compiles the native library and both wasm packages (`Stello.Web/wwwroot/engine` and `engine-mt`), about 1 min:
  ```
  cmd /c "dotnet build Stello.Net.slnx -c Release 2>&1" | Select-Object -Last 5
  dotnet test Stello.Engine.Tests -c Release --no-build
  dotnet test Egaroucid.Engine.Tests -c Release --no-build
  dotnet test Stello.Net.Tests -c Release --no-build
  ```
  `Stello.Net.Tests` has 2 known failures (section 7).
- **Node** (from the repo root, after the .NET build): `node --test Stello.Rust/stello-wasm/tests/engine.test.mjs Stello.Rust/stello-wasm/tests/engine-mt.test.mjs`. `npx` (a `.ps1`) is blocked by the execution policy; use `npx.cmd` if needed.
- **Benchmarks** (from `Stello.Net`):
  ```
  dotnet tools/Stello.Benchmark/bin/Release/net10.0/Stello.Benchmark.dll mid-bench --depth 10 --passes 1 --compare ../Plans/benchmark-results/v3.0-ref/<file>.json
  ```
  Use `solve-bench --from 40 --to 49` the same way.
- **Timing before/after** (the Windows version of `taskset -c 2`). Build the old commit in a scratch worktree and time both CLIs, alternating, pinned to core 2:
  ```
  $wt="<session folder>\files\wt-head"; git worktree add --detach $wt HEAD
  cd "$wt\Stello.Rust"; $env:CARGO_TARGET_DIR="$wt\target-rs"; cmd /c "cargo build --release -p stello-cli 2>&1"
  cmd /c "start `"`" /affinity 4 /wait /b `"$wt\target-rs\release\stello.exe`" solve-bench --from 40 --to 49 2>&1"
  git worktree remove --force $wt
  ```
- **Load and throttling** (instead of `uptime` and `/proc/cpuinfo`):
  ```
  (Get-CimInstance Win32_Processor).LoadPercentage
  (Get-Counter '\Processor Information(_Total)\% Processor Performance').CounterSamples.CookedValue
  (Get-Date) - (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
  ```
  The second line is the CPU speed as a percentage of base clock (above 100 = turbo). `Win32_Processor.CurrentClockSpeed` always shows the 2,100 MHz base clock, so it can't detect throttling.
- **Matches:**
  ```
  … match --a time-ms=500,name=A --b time-ms=500,name=B,opt=<key>=on --workers 12 --name <name>
  ```
  For 2 s add `--max-starts 300`, or `--starts <file>`.
- **Web check:** a Release AOT publish:
  ```
  dotnet publish Stello.Web/Stello.Web.csproj -c Release -o <dir> -p:StelloNativeEngine=false
  ```
  It needs a server with the COOP/COEP headers, plus Playwright. Neither is set up yet: the user allows `pip install playwright` (then `python -m playwright install chromium`) and small helper scripts in the session folder.
- **Links:** `python -I <session folder>\files\linkcheck.py docs README.md Plans/making-stello-3-1-findings.md Plans/making-stello-3-1-plan.md`. It checks local files and `#anchors`. Session folders don't carry over, so a new session must recreate it.
- **Git:**
  ```
  git -c user.name=cjp-dev -c user.email=cjp230364@gmail.com commit -F <message file or ->
  git fsck --full --no-dangling
  ```
  The local git identity is a different one (work), so always pass `-c user.name/-c user.email`. End commit messages with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. **Don't push:** the user always pushes from a bash terminal in the sandbox, where the repository is mounted at `/c/Udvikling/Privat/Spil/stello` (lowercase `stello`). There, `GH_TOKEN` must be unset first so that git uses the `gh` login (cjp-dev). Give the push command in this form:
  ```
  cd /c/Udvikling/Privat/Spil/stello
  unset GH_TOKEN
  git push origin feature/stello-3.1
  ```

## 7. Known problems and gotchas
- **Line endings:** `core.autocrlf=true` and `core.filemode=false`. The "LF will be replaced by CRLF" warnings are harmless (`.gitattributes` normalizes), and the sandbox's mode-755 problem doesn't occur here.
- **Windows-only test failures:**
  - 2 known `Stello.Net.Tests` score-display tests (`AnalysisViewModelTests.Update_ShowsTheSearchProgress`, `NewGuiFeaturesTests.AnalysisViewModel_SpeedAndUpdatedEvent`) expect `+40` where the app shows `+0.4 discs`; phase 3 fixes them.
  - 2 Egaroucid-port wall-clock tests and the Rust 8-thread time-per-move test fail when the machine is loaded or throttled. Check the processor performance (section 6); after a reboot they pass.
- **Toolchain:** the Rust `wasm32-unknown-unknown` target and `wasm-bindgen 0.2.130` are installed (checked 2026-11-07). The full build no longer needs `-p:StelloWasmEngine=false`. Python Playwright is not installed.
- **Never commit:** training data, `Egaroucid-main`, Egaroucid resources, or binaries (`target/`, `wwwroot/engine*/`, `.so/.dll/.wasm`); `*.log` is git-ignored.
- **Before/after builds:** use `git worktree add` in the session folder rather than `git stash` (the working tree may hold work in progress), then `git worktree remove --force`.
- **The opening book** holds 2,988 exact values found with a 60 s limit. Some may be bounds from stopped solves (phase 1 item 1). They are marked for the phase 11 recalculation.
- **Old plan files** (`Plans/stello-exhaustive-docs-and-code-audit-plan.md` and others) have pre-existing broken links, fixed in phase 4.

## 8. Next steps
1. **Wait for the user** to report phase 2 item 7 from the Windows agent. Review it as for items 1–6: the diff, the cargo/.NET/Node tests, the strict baseline, a speed spot check if nodes change, the findings. Clean up (switch removed, ABI 3), then commit, and end with the review and the bash push command for the user (section 6, Git).
2. **Items 8 and 9 the same way.** Item 9 changes only counting: no match is needed, but the node counts change, so the baseline is regenerated with a note.
3. **Gate P2:** agree with the user how to compare all kept fixes against `v3.0`. Then give a numbered match list (500 ms ×1,180 and 2 s ×600, about 22 and 45 min) and run it on the user's go. Record the results, and mark phase 2 done in the plan and findings.
4. **Phase 3, the apps:**
   - full WPF alignment (template: `Connect-4/Connect4.Net/Connect4.WPF/MainWindow.xaml`);
   - Computer vs Computer in both apps;
   - the two score-display tests.
5. **Then phases 4 and 5:** the 3.1 release, asking the user before fast-forwarding main.