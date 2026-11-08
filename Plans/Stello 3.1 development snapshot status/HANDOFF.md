# Handoff: Stello 3.1 development (snapshot 2026-11-08)

For the next session. Repository: `C:\Udvikling\Privat\Spil\Stello` on the user's Windows machine (GitHub `cjp-dev/Stello`). Branch **`feature/stello-3.1`**, phases 2 and 3 done and committed; the user pushes (section 6, Git).

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
- **Phase 2, items 1–9:**

  | Item | Commit | Result |
  |---|---|---|
  | 1. An upper bound equal to alpha cuts off | `7d01e75` | kept; 500 ms match 50.2 % |
  | 2. Upper-bound hash replies in the response table | `bbe2a60` | harmless; left as is |
  | 3a. TT aging (entries with more empties than the root count as empty) | `7a96a59` | kept; 500 ms 50.6 %, 2 s 47.5 % (starts 1–300), confirmation 2 s 51.6 % (starts 301–590) |
  | 3b. Keeping tighter bounds | — | dropped: no gain |
  | 4. `end_deep` keeps the move of its best ETC fail-low | `0eb00cc` | kept; 500 ms 50.3 %; FFO #43/#44 node counts changed |
  | 5. Moves decided by the local table leave the sort (`undone_first`) | `db01cf2` | kept; 500 ms 49.6 % (48.0–51.1); FFO #40–44 node counts changed; speed unchanged |
  | 6. Response ordering: the hash move is swapped to the front, and the loop stops one move early | — | harmless, no fix: the swap only changes ties; a rotate searched +0.02–0.05 % more midgame nodes; a single last move needs no sort |
  | 7. A full window for every root move up to depth 3 | — | intended, no fix: at depths 1–3 it is plain alpha-beta; PVS at every depth changed midgame nodes −0.4 % to +0.43 %, with no consistent gain |
  | 8. The Multi-ProbCut shallow search uses the hash table at the same ply | — | harmless, no fix: entries keep their real depth; turning the shallow stores off cost +4.7 % nodes (depth 10) and +6.2 % (depth 12) |
  | 9. Counting: one `count_node` path with the abort check every 1,024 nodes; terminal scores count as evaluations | `bbeea17` | kept, no match (statistics and reachable abort checks); FFO #40–44 counted nodes +1.23 % (93,697,164), midgame unchanged; solver about 1.4 % slower, midgame unchanged |

## 3. Phases 3 and 4 done (Gate P4 passed 2026-11-08); phase 5 next
Phase 3 runs in seven steps. The Windows agent does one step at a time and stops; this session reviews and commits each step, and the user starts the next step.
1. The two score-display tests: done.
2. Done: the shared foundation in Stello.App (see the [phase 3 findings](../making-stello-3-1-findings.md#step-2-shared-app-foundation-2026-11-07)); MainViewModel notifications, active-player flags and paired history rows, one transposition-table choice list, and shared appearance/sound preferences.
3. Done: Computer vs Computer in both apps; clocks, timeouts, Move Now, stopping, Undo and Save are covered.
4. Done: WPF alignment A: menus, buttons and shortcuts, the status panel and player cards, move history, analysis, and settings wording.
5. Done: WPF alignment B: shared board styles and web textures, 3D discs/flip animation, sound (`ISoundService` on SoundWaves), and F1/Shift+F1 Help links. The web's view-model appearance now stays synchronized with AppearanceStore and AppSettings.
6. Done: keep the frozen `docs/stello-1/` archive on GitHub, not in the GameShell viewer. All 24 links (14 distinct targets) retain anchors and safe new-tab attributes; all nine distinct GitHub URLs return HTTP 200. No code or GameShell changes.
7. Done: the Release web publish loads in headless Chromium at 844×390 with COOP/COEP headers and no horizontal overflow or browser errors. The page scrolls to the analysis and history below the fold, so no CSS fix was needed. Release build and all .NET test suites pass (489/489, 42/42, 115/115). Gate P3 awaits the user's WPF check.

Gate P3 passed (2026-11-08): the user checked WPF on Windows, the headless Chromium check passed, and docs 14/15 describe both apps.

Gate P2 (done):
- All kept phase 2 fixes (items 1, 3a, 4 and 5) were tested together against 3.0 using the temporary `v30_search` switch.
- At 500 ms, 3.1 scored 51.1% (95% interval 49.5–52.8, +8 Elo) over 1,180 games; at 2 s it scored 49.8% (47.9–51.7, −1 Elo) over 600 games. Neither result was significant; both pass the agreed not-worse criterion. See the [findings](../making-stello-3-1-findings.md#gate-p2-all-kept-fixes-against-30-2026-11-07) and the [500 ms](../benchmark-results/match_p2-gate-500ms.md) / [2 s](../benchmark-results/match_p2-gate-2s.md) reports.
- The temporary comparison switch was reviewed and removed after the matches; the ABI is back to 3.

### Phase 4: tools and verification gaps
- **Step 1 done (2026-11-08):** repaired the 124 broken links in the three old plans and replaced the copied GUI specification with a GameShell pointer. Five more links into the git-ignored Egaroucid checkout (chapter 06 and the 3.0 plan) now go to upstream GitHub. The link check finds 0 broken links in Plans and docs (125 and 4 before, in a clean worktree). The Release build has 0 warnings/errors, and the .NET tests pass 489/489, 42/42 and 115/115.
- **Step 2 done (2026-11-08):** default match workers are max(1, physical cores / max threads of the two sides); explicit `--workers` takes precedence. The Windows machine detects 16 physical cores (24 logical), giving 16 workers at 1 thread and 2 at 8 threads. A restricted-affinity check and 24 new unit-test cases pass; Release build has 0 warnings/errors and the .NET suites pass 489/489, 66/66 and 115/115. Linux topology has synthetic tests; Linux/macOS detection has not been run on those OSes here.
- **Step 3 done (2026-11-08):** each finished game is appended durably to `match_<name>.games.jsonl`. `--resume` validates sides, limits, seeds and ordered starts, then plays only missing games; reports are rebuilt from all saved games. Tiny-depth interruption/resume, mismatch, overwrite and partial-line recovery tests pass. Release build has 0 warnings/errors; tests pass 489/489, 72/72 and 115/115. No real matches were run.
- **Step 4 done (2026-11-08):** Egaroucid match sides accept `time-strict=on|off`; on is the default. Strict mode charges the book check to the move budget, caps clog search, and returns a legal fallback without further search if the deadline expires. Direct AI calls remain non-strict for C++ parity. Release build has 0 warnings/errors; tests pass 489/489, 76/76 and 115/115. The estimated 100 ms and 500 ms match list is in the [findings](../making-stello-3-1-findings.md#step-4-egaroucid-port-time-2026-11-08); no matches were run.
- **Step 5 done (2026-11-08):** C# and Rust select FFO #50–#59. With the user's go, one Rust run solved all ten correctly in 690.4 s and 25,419,468,677 nodes. No full C# run was made and no cited C++ timings for #50–59 were found. See the [findings](../making-stello-3-1-findings.md#step-5-ffo-range-and-sample-2026-11-08).
- **Step 6 done and reviewed (2026-11-08):** the first timed search on each worker was normally side A's. The match driver now warms both players without books before recording moves and resets their state; pass and single-move starts use a searchable fallback. In the user-run 500 ms, 20-game match, A's longest move was 508.1 ms and B's 500.7 ms; the supplied command requested two workers. Release build has 0 warnings/errors; tests pass 497/497, 78/78 and 115/115. See the [findings](../making-stello-3-1-findings.md#step-6-side-a-cold-start-timing-2026-11-08).
- **Step 7 documentation done (2026-11-08; awaiting review):** recorded existing Windows checks (Stello.Net.Tests 115/115, Rust/MSVC Release tests in `target/windows`, threaded web build and Node 15/15, and the 844×390 Chromium Release-publish check). The [findings](../making-stello-3-1-findings.md#step-7-platform-checks-2026-11-08) contain the user's manual Firefox/Safari checklist and empty result lines. Use the live site only after phase 5 deploys 3.1, or an existing local Release publish with COOP/COEP. No new runs, browser automation or installations. Gate P4 is next after this step's review; manual browser results remain for the user.

## 4. How we work (user decisions)
- **Who does what:** an agent on the user's Windows machine implements each item. This session reviews it: the diff, the tests, the baseline, the findings, plus its own spot checks. It fixes review issues and **commits**. The user is the one that does the Git **push**. The session is not allowed to do a Git **Push**. If its needed it have to ask the user. The user reports back after each item. When a item is reviewed and comitted, finish with your rewiew and a commandline showing the commando for the final push.
- **Matches only on the user's go:** give a numbered list with time estimates (see memory `feedback-long-matches-on-go`). At 12 workers on 1 thread:
  - 500 ms, 1,180 games: about 22 min;
  - 2 s, 600 games: about 40–45 min;
  - 1 min per game: about 3 h;
  - 5 min per game, 600 games: about 7.5 h.

  At 8 threads use 2 workers (500 ms: about 2 h). Check the load first (section 6), and run nothing heavy during a match. Matches now save each finished game to a JSONL log; resume with the same `--name` and `--resume` after repeating the same sides, limits, seeds and ordered starts. The tool rejects mismatches and recreates the reports from the saved games.
- **Fixes are kept unless they play worse** (upper bound above 50 % at 500 ms; confirm at 2 s if it matters). A new strategy, not a fix, must show a gain. If a significant loss shows no mechanism in the statistics, confirm it on the disjoint start positions 301–590 before dropping a fix.
- **A/B switches:** each change is temporarily a `SearchOptions` switch through Rust → FFI (`stello_engine_set_*`, an ABI bump) → C# internal `SearchOptions` → match key `opt=…`. It is **removed as soon as the match is approved**, and the ABI goes back to 3 (it is 3 now). For gate P2 a combined `v30_search` switch played 3.0; it is removed too.
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
- **Tests:** `Stello.Engine.Tests` (489), `Stello.Net.Tests` (Windows-only, 115), `Egaroucid.Engine.Tests` (76), the Rust tests (83), the Node tests (15).
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
  Phase 3 steps 1–2 pass Stello.Net.Tests 98/98; step 3 brings the count to 104/104.
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
- **WPF checks without a person at the screen:** start `Stello.Net\Stello.Net\bin\Release\net10.0-windows\Stello.Net.exe`, drive the menus with UI Automation (`UIAutomationClient`: find by name, expand or invoke), and capture the window with `PrintWindow` (flag 2) after `SetProcessDPIAware`. Back up `%AppData%\Stello\settings.json` first and restore it afterwards, and end the app with `$p.Kill()`, because the tool needs a literal process ID for `Stop-Process`. Save screenshots in the session folder, not the repository.
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
  - The 2 former `Stello.Net.Tests` score-display failures are fixed (phase 3 step 1); steps 1–2 pass 98/98 and step 3 passes 104/104.
  - Phase 3 step 4 and its status-header review adjustment pass 107/107.
  - Phase 3 step 5 and the Settings/Analysis follow-ups pass 115/115; Release build has 0 warnings/errors, engine suites pass 489/489 and 42/42, and Node tests pass 15/15. Browser appearance persistence was checked; desktop visual/audio checks remain for the user.
  - 2 Egaroucid-port wall-clock tests and the Rust 8-thread time-per-move test fail when the machine is loaded or throttled. Check the processor performance (section 6); after a reboot they pass.
- **Toolchain:** the Rust `wasm32-unknown-unknown` target and `wasm-bindgen 0.2.130` are installed (checked 2026-11-07). The full build no longer needs `-p:StelloWasmEngine=false`. Python Playwright is not installed.
- **Never commit:** training data, `Egaroucid-main`, Egaroucid resources, or binaries (`target/`, `wwwroot/engine*/`, `.so/.dll/.wasm`); `*.log` is git-ignored.
- **Before/after builds:** use `git worktree add` in the session folder rather than `git stash` (the working tree may hold work in progress), then `git worktree remove --force`.
- **The opening book** holds 2,988 exact values found with a 60 s limit. Some may be bounds from stopped solves (phase 1 item 1). They are marked for the phase 11 recalculation.
- **Old plan links:** fixed in phase 4 step 1; both Plans and docs have 0 broken local file/anchor links.

## 8. Next steps
1. **Phase 5, the 3.1 release** (plan, phase 5): the final matches on the user's go (3.1 against 3.0 at 100 ms, 500 ms and 2 s; 8 threads; against the port with time-strict on at 100 and 500 ms), the docs (a new chapter 27, About shows 3.1), then fast-forwarding main with the user's OK and the live check. Phase 4 is done; see the [findings](../making-stello-3-1-findings.md#gate-p4-2026-11-08).
2. **After the deploy:** the user runs the phase 4 step 7 Firefox/Safari checklist on the live site; record the results in the step 7 findings.