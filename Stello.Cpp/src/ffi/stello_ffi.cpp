#include "ffi/stello_ffi.hpp"

#include "core/board.hpp"
#include "core/version.hpp"
#include "eval/weights.hpp"
#include "search/engine.hpp"
#include "search/limits.hpp"
#include "search/options.hpp"

#include <memory>
#include <optional>
#include <span>
#include <utility>

struct StelloEngine {
    stello::search::SearchEngine engine;
    uint32_t threads{1};

    explicit StelloEngine(uint32_t hash_bits, stello::SearchOptions options)
        : engine(hash_bits, std::move(options)), threads(1) {}
};

struct StelloSignals {
    // Only ever allocated non-const by stello_signals_create(), so mutating through
    // stello_signals_{cancel,move_now,reset}(const StelloSignals*) is safe.
    stello::search::SearchSignals signals;
};

namespace {

inline void set_status(int32_t* status, int32_t value) noexcept {
    if (status != nullptr) {
        *status = value;
    }
}

inline uint64_t to_nanoseconds(stello::Duration duration) noexcept {
    const auto ticks = duration.count();
    return ticks <= 0 ? uint64_t{0} : static_cast<uint64_t>(ticks);
}

inline StelloSearchResult to_native_result(const stello::SearchResult& result) noexcept {
    StelloSearchResult native{};
    native.nodes = result.nodes;
    native.evaluations = result.evaluations;
    native.elapsed_ns = to_nanoseconds(result.elapsed);
    native.mv = result.mv.square.has_value()
        ? static_cast<int32_t>(result.mv.square->index())
        : -1;
    native.score = result.score;
    native.kind = static_cast<int32_t>(result.kind);
    native.depth = result.depth;
    return native;
}

} // namespace

extern "C" {

STELLO_API uint32_t stello_version(void) {
    return stello::kEngineVersionCode;
}

STELLO_API uint32_t stello_abi_version(void) {
    return stello::kAbiVersion;
}

STELLO_API StelloEngine* stello_engine_create(
    int32_t hash_bits,
    int32_t selective,
    int32_t probcut_percent,
    int32_t soft_time_percent,
    const uint8_t* eval,
    size_t eval_length,
    int32_t* status
) {
    StelloEngine* created = nullptr;
    int32_t code = STELLO_INTERNAL_ERROR;
    try {
        if (hash_bits < 10 || hash_bits > 26
            || probcut_percent < -1 || probcut_percent > 100'000
            || soft_time_percent < -1 || soft_time_percent > 100) {
            set_status(status, STELLO_INVALID_ARGUMENT);
            return nullptr;
        }

        std::shared_ptr<const stello::eval::EvalWeights> pattern;
        if (eval != nullptr && eval_length > 0) {
            auto weights = stello::eval::EvalWeights::read(std::span<const uint8_t>(eval, eval_length));
            if (!weights.has_value()) {
                set_status(status, STELLO_INVALID_EVALUATION);
                return nullptr;
            }
            pattern = std::make_shared<const stello::eval::EvalWeights>(std::move(*weights));
        }

        stello::SearchOptions options;
        options.pattern = std::move(pattern);
        if (soft_time_percent >= 0) {
            options.soft_time_percent = soft_time_percent;
        }
        options.selective = (selective != 0);
        if (probcut_percent >= 0) {
            options.probcut_percent = probcut_percent;
        }

        created = new StelloEngine(static_cast<uint32_t>(hash_bits), std::move(options));
        code = STELLO_OK;
    } catch (...) {
        delete created;
        created = nullptr;
        code = STELLO_INTERNAL_ERROR;
    }

    set_status(status, code);
    return created;
}

STELLO_API void stello_engine_destroy(StelloEngine* engine) {
    if (engine != nullptr) {
        try {
            delete engine;
        } catch (...) {
        }
    }
}

STELLO_API int32_t stello_engine_clear_hash(StelloEngine* engine) {
    if (engine == nullptr) {
        return STELLO_INVALID_ARGUMENT;
    }
    try {
        engine->engine.clear_hash();
        return STELLO_OK;
    } catch (...) {
        return STELLO_INTERNAL_ERROR;
    }
}

STELLO_API int32_t stello_engine_set_threads(StelloEngine* engine, int32_t threads) {
    if (engine == nullptr
        || threads < 1
        || threads > static_cast<int32_t>(stello::SearchLimits::MAX_THREADS)) {
        return STELLO_INVALID_ARGUMENT;
    }
    try {
        engine->threads = static_cast<uint32_t>(threads);
        return STELLO_OK;
    } catch (...) {
        return STELLO_INTERNAL_ERROR;
    }
}

STELLO_API int32_t stello_engine_set_ybwc(StelloEngine* engine, int32_t ybwc) {
    if (engine == nullptr) {
        return STELLO_INVALID_ARGUMENT;
    }
    try {
        engine->engine.set_ybwc(ybwc != 0);
        return STELLO_OK;
    } catch (...) {
        return STELLO_INTERNAL_ERROR;
    }
}

STELLO_API int32_t stello_engine_set_iteration_done(
    StelloEngine* engine,
    StelloIterationCallback callback,
    void* user
) {
    if (engine == nullptr) {
        return STELLO_INVALID_ARGUMENT;
    }
    try {
        if (callback == nullptr) {
            engine->engine.set_iteration_done({});
        } else {
            engine->engine.set_iteration_done([callback, user](int32_t depth, int32_t score) {
                callback(depth, score, user);
            });
        }
        return STELLO_OK;
    } catch (...) {
        return STELLO_INTERNAL_ERROR;
    }
}

STELLO_API StelloSignals* stello_signals_create(void) {
    try {
        return new StelloSignals{};
    } catch (...) {
        return nullptr;
    }
}

STELLO_API void stello_signals_destroy(StelloSignals* signals) {
    if (signals != nullptr) {
        try {
            delete signals;
        } catch (...) {
        }
    }
}

STELLO_API void stello_signals_cancel(const StelloSignals* signals) {
    if (signals != nullptr) {
        try {
            const_cast<StelloSignals*>(signals)->signals.cancel();
        } catch (...) {
        }
    }
}

STELLO_API void stello_signals_move_now(const StelloSignals* signals) {
    if (signals != nullptr) {
        try {
            const_cast<StelloSignals*>(signals)->signals.move_now();
        } catch (...) {
        }
    }
}

STELLO_API void stello_signals_reset(const StelloSignals* signals) {
    if (signals != nullptr) {
        try {
            const_cast<StelloSignals*>(signals)->signals.reset();
        } catch (...) {
        }
    }
}

STELLO_API int32_t stello_engine_search(
    StelloEngine* engine,
    uint64_t black,
    uint64_t white,
    int32_t player,
    int32_t mode,
    int32_t depth,
    int64_t time_ms,
    uint64_t only_moves,
    int32_t has_only_moves,
    const StelloSignals* signals,
    StelloProgressCallback progress,
    void* progress_user,
    StelloSearchResult* result
) {
    if (engine == nullptr || result == nullptr || (black & white) != 0) {
        return STELLO_INVALID_ARGUMENT;
    }

    if (player != 0 && player != 1) {
        return STELLO_INVALID_ARGUMENT;
    }
    if (mode < 0 || mode > 3) {
        return STELLO_INVALID_ARGUMENT;
    }

    auto limits = stello::SearchLimits::from_parts(
        static_cast<stello::TimeControlMode>(mode), depth, time_ms);
    if (!limits.has_value()) {
        return STELLO_INVALID_ARGUMENT;
    }
    limits = limits->with_threads(engine->threads);
    if (!limits.has_value()) {
        return STELLO_INVALID_ARGUMENT;
    }

    try {
        const stello::search::StopSignal& stop = signals != nullptr
            ? static_cast<const stello::search::StopSignal&>(signals->signals)
            : static_cast<const stello::search::StopSignal&>(stello::search::no_stop());

        stello::search::ProgressCallback report;
        if (progress != nullptr) {
            report = [progress, progress_user](const stello::SearchInfo& info) {
                StelloSearchInfo native{};
                native.nodes = info.nodes;
                native.evaluations = info.evaluations;
                native.elapsed_ns = to_nanoseconds(info.elapsed);
                native.depth = info.depth;
                native.current_move = static_cast<int32_t>(info.current_move.index());
                native.best_move = info.best_move.has_value()
                    ? static_cast<int32_t>(info.best_move->index())
                    : -1;
                native.score = info.score;
                native.kind = static_cast<int32_t>(info.kind);
                native.reserved = 0;
                progress(&native, progress_user);
            };
        }

        const stello::Board board(black, white);
        const auto pl = (player == 0) ? stello::Player::Black : stello::Player::White;
        const std::optional<uint64_t> only = (has_only_moves != 0)
            ? std::optional<uint64_t>(only_moves)
            : std::nullopt;

        const auto outcome = engine->engine.search(board, pl, *limits, std::move(report), stop, only);
        if (!outcome.ok) {
            return outcome.error == stello::search::SearchError::Cancelled
                ? STELLO_CANCELLED
                : STELLO_NO_LEGAL_ONLY_MOVE;
        }

        *result = to_native_result(outcome.result);
        return STELLO_OK;
    } catch (...) {
        return STELLO_INTERNAL_ERROR;
    }
}

} // extern "C"
