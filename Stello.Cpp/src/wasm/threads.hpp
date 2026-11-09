#pragma once

#include <cstddef>
#include <functional>

namespace stello::wasm {

/// Spawner for threaded WebAssembly: queues `body` to a pool Web Worker waiting in `pool_worker()`.
bool web_spawner(size_t index, std::function<void()> body);

/// Number of pool Web Workers currently idle waiting for a thread task.
size_t idle_threads();

/// Entry loop of a pool Web Worker (`engine-pool-worker.js`): never returns.
[[noreturn]] void pool_worker();

} // namespace stello::wasm
