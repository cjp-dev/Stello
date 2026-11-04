# Stello

Stello is an Othello program. Claus Pedersen wrote it in C++ in 1992 and ported it to C# and .NET 10 in 2026. Since version 3.0 its search engine is written in Rust, and it can search on several threads. It runs as a Windows program (WPF) and in the browser (Blazor WebAssembly, with the engine compiled to WebAssembly).

- **Play in the browser:** https://polite-sky-0e9899910.3.azurestaticapps.net
- **How it works:** [docs](docs/README.md) describes the engine, the opening book, the tools and the apps.
- **Code:** the solution is [Stello.Net/Stello.Net.slnx](Stello.Net/Stello.Net.slnx); the Rust engine is the Cargo workspace [Stello.Rust](Stello.Rust). Building needs the .NET 10 SDK and Rust from [rustup](https://rustup.rs) (on Windows with Visual Studio's C++ build tools; for the web app also the `wasm32-unknown-unknown` target and `wasm-bindgen-cli` 0.2.130, and for its threaded engine the nightly Rust `nightly-2026-11-02` with `rust-src`; without that nightly the web app is built with the single-threaded engine only), see [chapter 14](docs/14-app-integration.md#how-each-app-gets-the-engine) and [threads in the browser](docs/14-app-integration.md#threads-in-the-browser).

## Licence

Stello is free software. You can redistribute it and modify it under the terms of the GNU General Public License version 3 only (GPL-3.0-only), as published by the Free Software Foundation. See [LICENSE](LICENSE).

Stello uses and ports work by others, among them Egaroucid and Edax. See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
