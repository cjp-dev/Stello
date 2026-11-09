# 29 – References

[Back to the index](README.md)

Articles, papers and source code on the internet that explain the techniques used by Stello in more depth. All links were opened and checked when they were added.

<!-- Keep grouped by chapter; add each external link used in a chapter here too. -->

## General

| Link | What you find there |
|---|---|
| [Reversi – Wikipedia](https://en.wikipedia.org/wiki/Reversi) | The rules, the start position, the square notation (a1 top-left) and scoring. |
| [Computer Othello – Wikipedia](https://en.wikipedia.org/wiki/Computer_Othello) | An overview of search, evaluation and opening books in Othello programs, and their history. |
| [Othello – Chess Programming Wiki](https://www.chessprogramming.org/Othello) | Othello programming: search, evaluation, bitboards, programs and papers. |
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | A short guide by the author of Zebra: search, selective search, evaluation, opening books, and fastest-first in the endgame. |
| [Edax on GitHub](https://github.com/abulmo/edax-reversi) | The source of a very strong open-source bitboard Othello engine, for comparison. |

## 01 Overview

| Link | What you find there |
|---|---|
| [Introduction to the MVVM Toolkit – Microsoft Learn](https://learn.microsoft.com/dotnet/communitytoolkit/mvvm/) | `CommunityToolkit.Mvvm`, used by the app's view models. |
| [GameShell on GitHub](https://github.com/cjp-dev/GameShell) | The shared web GUI library of the web app (menus, panels, dialogs, documentation viewer), a git submodule in `external/GameShell`. |

## 02 Board and squares, 03 Rules and move generation

| Link | What you find there |
|---|---|
| [Bitboards – Chess Programming Wiki](https://www.chessprogramming.org/Bitboards) | What bitboards are and the basic operations on them. |
| [Edax on GitHub](https://github.com/abulmo/edax-reversi) | Richard Delorme's open-source bitboard Othello engine (`board.c` contains the ancestral sequential and parallel bitboard shift-and-mask move generators). |
| [Perft for Reversi – Aart Bik](http://www.aartbik.com/MISC/reversi.html) | Perft numbers for Othello from the start position; Stello's perft test uses the same numbers up to depth 8. |

## 04 Evaluation

| Link | What you find there |
|---|---|
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Position evaluation": disk-square tables, mobility-based evaluation and pattern-based evaluation, the kind Stello uses. |
| [Logistello – Michael Buro](https://skatgame.net/mburo/log.html) | The program that made trained pattern evaluations standard in Othello, and its papers. |
| [Technical explanation – Egaroucid](https://www.egaroucid.nyanyan.dev/en/technology/explanation/) | Egaroucid's pattern evaluation, where Stello's four 10-square patterns come from. |
| [Training data – Egaroucid](https://www.egaroucid.nyanyan.dev/en/technology/train-data/) | The data Stello's evaluation weights were trained on. It may not be redistributed, so it is not in the repository; the trained weights are. As its terms ask: *Stello used Egaroucid's self-play data for training and testing its Othello AI.* |
| [Othello – Chess Programming Wiki](https://www.chessprogramming.org/Othello) | The "Evaluation" section, from IAGO's hand-made features to pattern evaluations. |

## 05 Move ordering

| Link | What you find there |
|---|---|
| [Alpha-Beta – Chess Programming Wiki](https://www.chessprogramming.org/Alpha-Beta) | "Savings": why searching the best move first gives the most cutoffs. |
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Move ordering": killer responses in Othello. |
| [Egaroucid on GitHub](https://github.com/Nyanyan/Egaroucid) | `src/engine/move_ordering.hpp`, the source of the evaluation ordering and its weights. |

## 06 Midgame search

| Link | What you find there |
|---|---|
| [Alpha-Beta – Chess Programming Wiki](https://www.chessprogramming.org/Alpha-Beta) | Alpha-beta, fail-soft and fail-hard, in negamax form. |
| [Principal Variation Search – Chess Programming Wiki](https://www.chessprogramming.org/Principal_Variation_Search) | PVS: null-window searches after the first move, and re-searches. |
| [ProbCut – Chess Programming Wiki](https://www.chessprogramming.org/ProbCut) | ProbCut and Multi-ProbCut, the statistical forms of the selective search idea; Stello uses Multi-ProbCut. |
| [Buro (1995): ProbCut: An Effective Selective Extension of the Alpha-Beta Algorithm (pdf)](https://skatgame.net/mburo/ps/probcut.pdf) | The original ProbCut paper. |
| [Negamax – Wikipedia](https://en.wikipedia.org/wiki/Negamax) | Negamax with alpha-beta and with a transposition table, in pseudocode. |
| [Fail-Soft – Chess Programming Wiki](https://www.chessprogramming.org/Fail-Soft) | Scores outside the window, and why they help. |
| [Null Window – Chess Programming Wiki](https://www.chessprogramming.org/Null_Window) | Null-window (zero-window) searches as a yes/no test. |
| [Iterative Deepening – Chess Programming Wiki](https://www.chessprogramming.org/Iterative_Deepening) | Iterative deepening for time control and move ordering. |
| [Odd-Even Effect – Chess Programming Wiki](https://www.chessprogramming.org/Odd-Even_Effect) | Why node counts and scores differ between odd and even depths. |
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Searching": alpha-beta, move ordering and selective search in Othello. |

## 07 Transposition table

| Link | What you find there |
|---|---|
| [Transposition Table – Chess Programming Wiki](https://www.chessprogramming.org/Transposition_Table) | What is stored, bounds, collisions, and replacement schemes, including the two-tier system Stello uses. |
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Transposition tables", with the tiger-opening example. |

## 08 Endgame solver

| Link | What you find there |
|---|---|
| [Egaroucid on GitHub](https://github.com/Nyanyan/Egaroucid) | `src/engine/endsearch_nws.hpp`, `endsearch_nws_last_generic.hpp`, `stability_cutoff.hpp` and `move_ordering.hpp`: the endgame code Stello's solver is adapted from. |
| [Technical explanation – Egaroucid](https://www.egaroucid.nyanyan.dev/en/technology/explanation/) | Egaroucid's endgame search, move ordering and the code for the last empty squares. |
| [Enhanced Transposition Cutoff – Chess Programming Wiki](https://www.chessprogramming.org/Enhanced_Transposition_Cutoff) | Looking up the children in the hash table before searching them; the solver does it from 12 empty squares. |
| [Null Window – Chess Programming Wiki](https://www.chessprogramming.org/Null_Window) | Null-window searches, which the solver is built from. |
| [endgame.c – radagast.se](http://radagast.se/othello/endgame.c) | A small endgame solver (Smith, Weill, Andersson) with region parity, a fixed square order, special code for the last squares and 112 test positions. |
| [The FFO endgame test suite – radagast.se](http://radagast.se/othello/ffotest.html) | The test positions #40–#59, their correct scores and moves, and Zebra's times. |
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Endgame": why ordering decides everything, and fastest-first. |
| [Edax on GitHub](https://github.com/abulmo/edax-reversi) | Another fast open-source bitboard endgame solver, for comparison. |

## 09 Time control

| Link | What you find there |
|---|---|
| [Iterative Deepening – Chess Programming Wiki](https://www.chessprogramming.org/Iterative_Deepening) | Iterative deepening as the basis of time management. |

## 10 Opening book

| Link | What you find there |
|---|---|
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Opening knowledge": why an opening book pays off, and how it can be built. |
| [Computer Othello – Wikipedia](https://en.wikipedia.org/wiki/Computer_Othello) | "Opening book": how Othello programs use opening books. |

## 11 Book learning

| Link | What you find there |
|---|---|
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Opening knowledge": adding the best move that no game has played to every book position, then minimaxing the book. The same idea as Stello's dropout expansion. |

## 14 App integration

| Link | What you find there |
|---|---|
| [Introduction to the MVVM Toolkit – Microsoft Learn](https://learn.microsoft.com/dotnet/communitytoolkit/mvvm/) | The library behind `ObservableObject`, `[ObservableProperty]` and the commands. |
| [RelayCommand attribute – Microsoft Learn](https://learn.microsoft.com/dotnet/communitytoolkit/mvvm/generators/relaycommand) | Generated commands, `CanExecute`, `NotifyCanExecuteChangedFor`, and why an async command is disabled while it runs. |
| [Progress&lt;T&gt; class – Microsoft Learn](https://learn.microsoft.com/dotnet/api/system.progress-1) | Callbacks run on the `SynchronizationContext` captured when the object is created. |
| [Cancellation in managed threads – Microsoft Learn](https://learn.microsoft.com/dotnet/standard/threading/cancellation-in-managed-threads) | Cooperative cancellation with `CancellationTokenSource` and `OperationCanceledException`. |

## 15 GUI and settings

| Link | What you find there |
|---|---|
| [GameShell on GitHub](https://github.com/cjp-dev/GameShell) | The shared web GUI library and its general game GUI specification, which the web app follows. |
| [World Othello Federation (WOF)](https://www.worldothello.org/) | Official tournament rules, clock specifications, first-mover (Black) conventions, and ratings. |
| [Logistello – Michael Buro](https://skatgame.net/mburo/log.html) | Michael Buro's historic championship Othello program and evaluation papers. |
| [Usability 101: Introduction to Usability – Nielsen Norman Group](https://www.nngroup.com/articles/usability-101-introduction-to-usability/) | Principles of status visibility, match integrity, and user control. |

## 17 Measuring the engine

| Link | What you find there |
|---|---|
| [The FFO endgame test suite – radagast.se](http://radagast.se/othello/ffotest.html) | The positions #40–#59 of `solve-bench`, with their correct scores and moves. |
| [Edax on GitHub](https://github.com/abulmo/edax-reversi) | `problem/fforum-40-59.obf`, the same positions in Edax's format, one of the two sources of `ffo-40-59.txt`. |

## 18 The Egaroucid port, 19 Stello 1 against Egaroucid

| Link | What you find there |
|---|---|
| [Egaroucid on GitHub](https://github.com/Nyanyan/Egaroucid) | Takuto Yamana's Othello program (GPL-3.0-or-later), whose engine 7.9 Stello ports to C# (`Egaroucid.Engine`). |
| [Egaroucid – website](https://www.egaroucid.nyanyan.dev/en/) | Downloads, including the resources the port reads (evaluation weights, hash numbers, opening book). |
| [Technical explanation – Egaroucid](https://www.egaroucid.nyanyan.dev/en/technology/explanation/) | How Egaroucid works: pattern evaluation, Multi-ProbCut, move ordering and the endgame search. |
| [Training data – Egaroucid](https://www.egaroucid.nyanyan.dev/en/technology/train-data/) | Egaroucid's self-play and match data. Stello's `eval-compare` uses it to measure evaluations; it may not be redistributed. As its terms ask: *Stello used Egaroucid's self-play data for training and testing its Othello AI.* |
| [ProbCut – Chess Programming Wiki](https://www.chessprogramming.org/ProbCut) | Multi-ProbCut, the selective search Egaroucid uses. |

## 20–23 From Stello 1 to Stello 2.0

| Link | What you find there |
|---|---|
| [Training data – Egaroucid](https://www.egaroucid.nyanyan.dev/en/technology/train-data/) | The data Stello 2.0's evaluation was trained on (`Stello.Trainer`). It may not be redistributed, so it is not in the repository; the trained weights are. As its terms ask: *Stello used Egaroucid's self-play data for training and testing its Othello AI.* |
| [Technical explanation – Egaroucid](https://www.egaroucid.nyanyan.dev/en/technology/explanation/) | Egaroucid's pattern evaluation and its training with Adam, move ordering, Multi-ProbCut and the endgame search, which Stello 2.0 follows. |
| [Kingma and Ba (2014): Adam: A Method for Stochastic Optimization – arXiv](https://arxiv.org/abs/1412.6980) | The optimiser of the trainer. |
| [Logistello – Michael Buro](https://skatgame.net/mburo/log.html) | The program that brought trained pattern evaluation and Multi-ProbCut to Othello. |
| [Buro (1995): ProbCut: An Effective Selective Extension of the Alpha-Beta Algorithm (pdf)](https://skatgame.net/mburo/ps/probcut.pdf) | The original ProbCut paper, the basis of the Multi-ProbCut of phase 5. |

## 25 Phases 0–4: the engine in Rust

| Link | What you find there |
|---|---|
| [Rust](https://www.rust-lang.org/) | The language of Stello 3.0's search core. |
| [rustup](https://rustup.rs/) | The Rust installer, needed to build the apps (chapter 14). |
| [The Cargo Book](https://doc.rust-lang.org/cargo/) | Rust's build tool: workspaces, profiles (LTO, `panic = "abort"`), targets. |
| [FFI – the Rustonomicon](https://doc.rust-lang.org/nomicon/ffi.html) | Calling Rust from other languages through the C ABI, as `stello-ffi` does. |
| [P/Invoke source generation – Microsoft Learn](https://learn.microsoft.com/dotnet/standard/native-interop/pinvoke-source-generation) | `[LibraryImport]`, which `Stello.Engine` uses to call the native library. |
| [The wasm-bindgen Guide](https://wasm-bindgen.github.io/wasm-bindgen/) | How Rust and JavaScript call each other in WebAssembly; the `--target web` package of `stello-wasm`. |
| [wasm-bindgen on GitHub](https://github.com/wasm-bindgen/wasm-bindgen) | The source and the prebuilt `wasm-bindgen-cli` releases that the deploy workflow downloads. |
| [WebAssembly](https://webassembly.org/) | The format the browser runs the engine in. |
| [Web Workers API – MDN](https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API) | The browser thread the engine runs in. |
| [SharedArrayBuffer – MDN](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer) | The shared memory of the stop signal, and why it needs cross-origin isolation. |
| [PEXT – x86 instruction reference](https://www.felixcloutier.com/x86/pext) | The BMI2 instruction the evaluation uses to read a feature's squares. |
| [BMI2 – Chess Programming Wiki](https://www.chessprogramming.org/BMI2) | BMI2 and its `pext` and `pdep`, and their slow versions on AMD before Zen 3. |
| [is_x86_feature_detected – Rust documentation](https://doc.rust-lang.org/std/arch/macro.is_x86_feature_detected.html) | The run-time check behind the choice of the fast path. |
| [Edax's flip_carry_64.c on GitHub](https://github.com/abulmo/edax-reversi/blob/master/src/flip_carry_64.c) | Per-square flip functions, an idea left for later. |

## 26 Phases 5–9: threads

| Link | What you find there |
|---|---|
| [Parallel Search – Chess Programming Wiki](https://www.chessprogramming.org/Parallel_Search) | An overview of the ways to search a game tree on several threads. |
| [Lazy SMP – Chess Programming Wiki](https://www.chessprogramming.org/Lazy_SMP) | Threads that search the same root at different depths and help each other only through the shared hash table (Julien Marcel, Daniel Homan and others, 2012–2013). |
| [Young Brothers Wait Concept – Chess Programming Wiki](https://www.chessprogramming.org/Young_Brothers_Wait_Concept) | YBWC: the younger brothers of a node are searched in parallel only after the oldest brother is done; with the history and the papers. |
| Rainer Feldmann, Burkhard Monien, Peter Mysliwietz, Oliver Vornberger (1989). *Distributed Game-Tree Search.* ICCA Journal, Vol. 12, No. 2, pp. 65–73 | The paper that introduced YBWC to a wider audience. |
| Rainer Feldmann (1993). *Game Tree Search on Massively Parallel Systems.* PhD thesis, University of Paderborn ([Rainer Feldmann – Chess Programming Wiki](https://www.chessprogramming.org/Rainer_Feldmann)) | YBWC in full, measured on hundreds of processors. |
| [Shared Hash Table – Chess Programming Wiki](https://www.chessprogramming.org/Shared_Hash_Table) | One hash table for all threads: locks, and Hyatt's lockless XOR check. |
| [Seqlock – Wikipedia](https://en.wikipedia.org/wiki/Seqlock) | The sequence lock that guards each slot of Stello's tables (chapter 07). |
| [Egaroucid's ai.hpp](https://github.com/Nyanyan/Egaroucid/blob/main/src/engine/ai.hpp), [ybwc.hpp](https://github.com/Nyanyan/Egaroucid/blob/main/src/engine/ybwc.hpp) and [thread_pool.hpp](https://github.com/Nyanyan/Egaroucid/blob/main/src/engine/thread_pool.hpp) on GitHub | The Lazy SMP helpers, the YBWC splits and the thread pool that Stello's threads are modelled on. |
| [std::thread::scope – Rust documentation](https://doc.rust-lang.org/std/thread/fn.scope.html) | Scoped threads, which may borrow from the caller; the model of the pool's scopes. |
| [Miri on GitHub](https://github.com/rust-lang/miri) and [ThreadSanitizer – Clang](https://clang.llvm.org/docs/ThreadSanitizer.html) | The two tools that checked the table and the pool for undefined behaviour and data races. |
| [WebAssembly threads proposal on GitHub](https://github.com/WebAssembly/threads) | Shared memory and atomic instructions in WebAssembly, which the threaded package uses. |
| [Atomics.wait – MDN](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Atomics/wait) | How a Web Worker waits on shared memory; not allowed on a page's main thread. |
| [crossOriginIsolated – MDN](https://developer.mozilla.org/en-US/docs/Web/API/Window/crossOriginIsolated) and [Making your website cross-origin isolated – web.dev](https://web.dev/articles/coop-coep) | Why `SharedArrayBuffer` needs the COOP and COEP headers. |
| [navigator.hardwareConcurrency – MDN](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/hardwareConcurrency) | The browser's thread count, the web app's Automatic setting. |
| [build-std – the Cargo Book](https://doc.rust-lang.org/cargo/reference/unstable.html#build-std) | Building the standard library with atomics, which the threaded WebAssembly package needs nightly Rust for. |
| [wasm-bindgen-rayon on GitHub](https://github.com/RReverser/wasm-bindgen-rayon) | The usual way to run Rust threads on Web Workers; Stello uses its own small pool instead (chapter 14). |

## Writing these documents

| Link | What you find there |
|---|---|
| [Mermaid](https://mermaid.js.org/) | The diagram language used in these documents. |
| [Creating diagrams – GitHub Docs](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/creating-diagrams) | How GitHub shows Mermaid diagrams. |
| [Writing mathematical expressions – GitHub Docs](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/writing-mathematical-expressions) | How GitHub shows math formulas. |

---

Previous: [28 Glossary](28-glossary.md) · Next: [Index](README.md)
