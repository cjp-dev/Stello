#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(STELLO_BUILDING_DLL)
#    define STELLO_API __declspec(dllexport)
#  elif defined(STELLO_USE_DLL)
#    define STELLO_API __declspec(dllimport)
#  else
#    define STELLO_API
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define STELLO_API __attribute__((visibility("default")))
#else
#  define STELLO_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// The call succeeded.
#define STELLO_OK 0
/// The search was cancelled (the cancel signal was set).
#define STELLO_CANCELLED 1
/// None of the moves in `only_moves` is legal.
#define STELLO_NO_LEGAL_ONLY_MOVE 2
/// An argument is not valid.
#define STELLO_INVALID_ARGUMENT 3
/// The evaluation file could not be read.
#define STELLO_INVALID_EVALUATION 4
/// The engine failed unexpectedly.
#define STELLO_INTERNAL_ERROR 5

typedef struct StelloEngine StelloEngine;
typedef struct StelloSignals StelloSignals;

/// Progress of a running search (`SearchInfo`), passed to the progress callback. 48 bytes.
typedef struct StelloSearchInfo {
    uint64_t nodes;
    uint64_t evaluations;
    uint64_t elapsed_ns;
    int32_t depth;
    int32_t current_move;
    int32_t best_move;
    int32_t score;
    int32_t kind;
    int32_t reserved;
} StelloSearchInfo;

/// Result of a search (`SearchResult`), 40 bytes.
typedef struct StelloSearchResult {
    uint64_t nodes;
    uint64_t evaluations;
    uint64_t elapsed_ns;
    int32_t mv;
    int32_t score;
    int32_t kind;
    int32_t depth;
} StelloSearchResult;

typedef void (*StelloProgressCallback)(const StelloSearchInfo* info, void* user);
typedef void (*StelloIterationCallback)(int32_t depth, int32_t score, void* user);

STELLO_API uint32_t stello_version(void);
STELLO_API uint32_t stello_abi_version(void);

STELLO_API StelloEngine* stello_engine_create(
    int32_t hash_bits,
    int32_t selective,
    int32_t probcut_percent,
    int32_t soft_time_percent,
    const uint8_t* eval,
    size_t eval_length,
    int32_t* status
);

STELLO_API void stello_engine_destroy(StelloEngine* engine);
STELLO_API int32_t stello_engine_clear_hash(StelloEngine* engine);
STELLO_API int32_t stello_engine_set_threads(StelloEngine* engine, int32_t threads);
STELLO_API int32_t stello_engine_set_ybwc(StelloEngine* engine, int32_t ybwc);
STELLO_API int32_t stello_engine_set_iteration_done(
    StelloEngine* engine,
    StelloIterationCallback callback,
    void* user
);

STELLO_API StelloSignals* stello_signals_create(void);
STELLO_API void stello_signals_destroy(StelloSignals* signals);
STELLO_API void stello_signals_cancel(const StelloSignals* signals);
STELLO_API void stello_signals_move_now(const StelloSignals* signals);
STELLO_API void stello_signals_reset(const StelloSignals* signals);

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
);

#ifdef __cplusplus
} // extern "C"

static_assert(sizeof(StelloSearchInfo) == 48);
static_assert(offsetof(StelloSearchInfo, depth) == 24);
static_assert(sizeof(StelloSearchResult) == 40);
static_assert(offsetof(StelloSearchResult, mv) == 24);
#endif
