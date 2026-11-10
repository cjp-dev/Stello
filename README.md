# Stello

Stello is an Othello program. Claus Pedersen wrote it in C++ in 1992 and ported it to C# and .NET 10 in 2026; since version 3.0 it searches on several threads, and since version 4.0 its search engine runs in modern C++20 (ported 1-to-1 from the Rust 3.0/3.1 rewrite). It runs as a Windows program (WPF) and in the browser (Blazor WebAssembly, with the engine compiled to WebAssembly).

- **Play in the browser:** https://othello.clauspedersen.net
- **How it works:** [docs](docs/README.md) (or online at https://othello.clauspedersen.net/docs) describes the engine, the opening book, the tools and the apps.
- **Code:** the solution is [Stello.Net/Stello.Net.slnx](Stello.Net/Stello.Net.slnx); the C++20 search engine is [Stello.Cpp](Stello.Cpp) (the preserved Rust 3.1 workspace in [Stello.Rust](Stello.Rust) is kept for comparisons and WebAssembly tests). Building needs the .NET 10 SDK, CMake, Ninja and Clang/LLVM (on Windows with Visual Studio's C++ build tools; for the web app also the `wasm-tools` .NET workload and the [Emscripten SDK](https://emscripten.org) for both single- and multi-threaded WebAssembly, or pass `-p:StelloWasmEngine=false` for C#-only checks), see [chapter 14](docs/14-app-integration.md#how-each-app-gets-the-engine) and [threads in the browser](docs/14-app-integration.md#threads-in-the-browser).

## Licence

Stello is free software. You can redistribute it and modify it under the terms of the GNU General Public License version 3 only (GPL-3.0-only), as published by the Free Software Foundation. See [LICENSE](LICENSE).

Stello uses and ports work by others, among them Egaroucid and Edax. See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
