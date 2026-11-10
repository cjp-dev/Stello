#include "core/version.hpp"
#include "eval/weights.hpp"
#include "search/engine.hpp"
#include "wasm/protocol.hpp"
#include "wasm/threads.hpp"

#include <emscripten/emscripten.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#ifdef __EMSCRIPTEN_PTHREADS__
#include <emscripten/stack.h>
#include <pthread.h>

extern "C" {
void* _emscripten_tls_init(void);
void _emscripten_thread_init(pthread_t, int, int, int, int, int);
void __set_thread_state(pthread_t, int, int, int);
}
#endif

namespace {

constexpr double kProgressIntervalMs = 100.0;
constexpr int32_t kSignalMoveNow = 1;
constexpr int32_t kSignalCancel = 2;

extern "C" {
__attribute__((import_module("env"), import_name("stello_js_progress")))
void stello_js_progress(const char* ptr, size_t len);

__attribute__((import_module("env"), import_name("stello_js_signal")))
int32_t stello_js_signal();

__attribute__((import_module("env"), import_name("stello_js_absolute_now_ms")))
double stello_js_absolute_now_ms();

__attribute__((import_module("env"), import_name("stello_js_performance_now_ms")))
double stello_js_performance_now_ms();
}

stello::Duration browser_clock() {
    const double ms = (std::max)(stello_js_absolute_now_ms(), 0.0);
    return std::chrono::duration_cast<stello::Duration>(std::chrono::duration<double, std::milli>(ms));
}

class HostStopSignal final : public stello::search::StopSignal {
public:
    bool is_cancelled() const noexcept override { return stello_js_signal() == kSignalCancel; }
    bool is_move_now() const noexcept override { return stello_js_signal() == kSignalMoveNow; }
};

struct ProgressPoster {
    bool has_last{false};
    double last_at_ms{0.0};
    int32_t last_depth{0};
    int32_t last_best_move{-1};
    stello::ScoreKind last_kind{stello::ScoreKind::None};

    void report(const stello::SearchInfo& info) {
        const double now = stello_js_performance_now_ms();
        const int32_t best_sq = info.best_move.has_value() ? static_cast<int32_t>(info.best_move->index()) : -1;
        if (has_last && last_depth == info.depth && last_best_move == best_sq && last_kind == info.kind
            && (now - last_at_ms) < kProgressIntervalMs)
        {
            return;
        }
        has_last = true;
        last_at_ms = now;
        last_depth = info.depth;
        last_best_move = best_sq;
        last_kind = info.kind;

        const std::string json = stello::wasm::write_progress(info);
        stello_js_progress(json.data(), json.size());
    }
};

struct EngineState {
    std::shared_ptr<const stello::eval::EvalWeights> pattern;
    std::unique_ptr<stello::search::SearchEngine> engine;
};

std::optional<EngineState> g_state;
std::string g_result;

std::unique_ptr<stello::search::SearchEngine> make_engine(
    uint32_t hash_bits,
    std::shared_ptr<const stello::eval::EvalWeights> pattern
) {
    stello::SearchOptions options;
    options.pattern = std::move(pattern);
    auto engine = std::make_unique<stello::search::SearchEngine>(hash_bits, std::move(options));
    engine->set_clock(&browser_clock);
#ifdef __EMSCRIPTEN_PTHREADS__
    engine->set_spawner(&stello::wasm::web_spawner);
#endif
    return engine;
}

#ifdef __EMSCRIPTEN_PTHREADS__
constexpr size_t kWorkerStackSize = 1048576; // 1 MiB per worker thread

std::atomic<int32_t> g_boot_lock{0};
alignas(16) uint8_t g_boot_stack[16384];
pthread_t g_main_pthread{0};

void* dummy_pthread_entry(void*) {
    return nullptr;
}
#endif

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE const char* stello_wasm_version() noexcept {
    return stello::kEngineVersion;
}

EMSCRIPTEN_KEEPALIVE uint32_t stello_wasm_threaded() noexcept {
#ifdef __EMSCRIPTEN_PTHREADS__
    return 1U;
#else
    return 0U;
#endif
}

EMSCRIPTEN_KEEPALIVE void* stello_wasm_alloc(size_t size) noexcept {
    return std::malloc(size == 0 ? 1 : size);
}

EMSCRIPTEN_KEEPALIVE void stello_wasm_free(void* ptr) noexcept {
    std::free(ptr);
}

EMSCRIPTEN_KEEPALIVE const char* stello_wasm_result_ptr() noexcept {
    return g_result.data();
}

EMSCRIPTEN_KEEPALIVE size_t stello_wasm_result_len() noexcept {
    return g_result.size();
}

EMSCRIPTEN_KEEPALIVE int32_t stello_wasm_init(
    int32_t has_eval,
    const uint8_t* eval_ptr,
    size_t eval_len,
    uint32_t hash_bits
) noexcept {
    try {
        g_result.clear();
        if (hash_bits < 10U || hash_bits > 26U) {
            g_result = "Hash bits must be 10-26, not " + std::to_string(hash_bits) + ".";
            return -1;
        }

        std::shared_ptr<const stello::eval::EvalWeights> pattern;
        if (has_eval != 0) {
            stello::eval::EvalError err;
            auto weights = stello::eval::EvalWeights::read(std::span<const uint8_t>(eval_ptr, eval_len), &err);
            if (!weights.has_value()) {
                g_result = err.message();
                return -1;
            }
            pattern = std::make_shared<stello::eval::EvalWeights>(std::move(*weights));
        } else {
            (void)stello::eval::EvalWeights::built_in();
        }

        auto engine = make_engine(hash_bits, pattern);
        g_state = EngineState{std::move(pattern), std::move(engine)};
        return 0;
    } catch (const std::exception& ex) {
        g_result = ex.what();
        return -1;
    } catch (...) {
        g_result = "Internal engine error.";
        return -1;
    }
}

EMSCRIPTEN_KEEPALIVE int32_t stello_wasm_search(const char* req_ptr, size_t req_len) noexcept {
    try {
        g_result.clear();
        const std::string_view json(req_ptr != nullptr ? req_ptr : "", req_len);
        const auto parsed = stello::wasm::read_request(json);
        if (!parsed.ok) {
            g_result = parsed.error;
            return -1;
        }
        if (!g_state.has_value()) {
            g_result = "The engine has not been initialised.";
            return -1;
        }

        if (g_state->engine->hash_bits() != parsed.request.hash_bits) {
            g_state->engine = make_engine(parsed.request.hash_bits, g_state->pattern);
        }

        ProgressPoster poster;
        auto report_cb = [&poster](const stello::SearchInfo& info) { poster.report(info); };
        const HostStopSignal stop_signal;
        const auto outcome = g_state->engine->search(
            parsed.request.board,
            parsed.request.player,
            parsed.request.limits,
            report_cb,
            stop_signal,
            std::nullopt);

        if (outcome.ok) {
            g_result = stello::wasm::write_result(outcome.result);
            return 0;
        }
        if (outcome.error == stello::search::SearchError::Cancelled) {
            return 1;
        }
        g_result = std::string(stello::search::search_error_message(outcome.error));
        return -1;
    } catch (const std::exception& ex) {
        g_result = ex.what();
        return -1;
    } catch (...) {
        g_result = "Internal engine error.";
        return -1;
    }
}

EMSCRIPTEN_KEEPALIVE uint32_t stello_wasm_idle_threads() noexcept {
#ifdef __EMSCRIPTEN_PTHREADS__
    return static_cast<uint32_t>(stello::wasm::idle_threads());
#else
    return 0U;
#endif
}

EMSCRIPTEN_KEEPALIVE void stello_wasm_pool_worker() noexcept {
#ifdef __EMSCRIPTEN_PTHREADS__
    stello::wasm::pool_worker();
#endif
}

#ifdef __EMSCRIPTEN_PTHREADS__
EMSCRIPTEN_KEEPALIVE void stello_wasm_record_main() noexcept {
    g_main_pthread = pthread_self();
}

EMSCRIPTEN_KEEPALIVE uintptr_t stello_wasm_boot_lock_ptr() noexcept {
    return reinterpret_cast<uintptr_t>(&g_boot_lock);
}

EMSCRIPTEN_KEEPALIVE uintptr_t stello_wasm_boot_stack_top() noexcept {
    return reinterpret_cast<uintptr_t>(g_boot_stack + sizeof(g_boot_stack));
}

EMSCRIPTEN_KEEPALIVE uintptr_t stello_wasm_worker_prepare() noexcept {
    // Give pthread_create a valid temporary __pthread_self() before switching to the worker's own pthread_t.
    __set_thread_state(g_main_pthread, 0, 0, 1);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, kWorkerStackSize);

    pthread_t pt{0};
    pthread_create(&pt, &attr, &dummy_pthread_entry, nullptr);
    pthread_attr_destroy(&attr);

    pthread_attr_t actual;
    pthread_getattr_np(pt, &actual);
    void* stack_addr = nullptr;
    size_t stack_size = 0;
    pthread_attr_getstack(&actual, &stack_addr, &stack_size);
    pthread_attr_destroy(&actual);

    const uintptr_t stack_base = reinterpret_cast<uintptr_t>(stack_addr);
    const uintptr_t stack_top = (stack_base + stack_size) & ~uintptr_t{15};

    _emscripten_thread_init(pt, 0, 0, 1, 0, 0);
    _emscripten_tls_init();
    emscripten_stack_set_limits(reinterpret_cast<void*>(stack_top), reinterpret_cast<void*>(stack_base));
    return stack_top;
}
#endif

} // extern "C"
