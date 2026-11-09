#pragma once

#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

#ifndef STELLO_ABORT_EXCEPTIONS
#define STELLO_ABORT_EXCEPTIONS 0
#endif

namespace stello::search {

/// Why a running search unwinds. C#: `OperationCanceledException` and `SearchAbortedException`.
enum class Abort : uint8_t {
    /// Time is up or Move Now: keep the best move found so far.
    Stop = 0,
    /// Cancelled: no result.
    Cancelled = 1,
};

struct AbortStatus {
    uint8_t aborted;
    Abort reason;

    static constexpr AbortStatus ok() noexcept { return {0, Abort::Stop}; }
    static constexpr AbortStatus err(Abort a) noexcept { return {1, a}; }
    constexpr bool is_ok() const noexcept { return aborted == 0; }
    constexpr bool is_err() const noexcept { return aborted != 0; }
    constexpr Abort abort_reason() const noexcept { return reason; }
};

template <typename T>
struct AbortResultBox {
    AbortStatus status;
    T value;

    constexpr bool is_ok() const noexcept { return status.aborted == 0; }
    constexpr bool is_err() const noexcept { return status.aborted != 0; }
    constexpr Abort abort_reason() const noexcept { return status.reason; }
};

static_assert(sizeof(AbortResultBox<int32_t>) == 8, "AbortResultBox<int32_t> must fit in a single 64-bit register");
static_assert(std::is_trivial_v<AbortStatus>);
static_assert(std::is_trivial_v<AbortResultBox<int32_t>>);
static_assert(std::is_aggregate_v<AbortStatus>);
static_assert(std::is_aggregate_v<AbortResultBox<int32_t>>);

struct ScoreMove {
    int32_t score;
    int32_t mv;

    friend constexpr bool operator==(const ScoreMove&, const ScoreMove&) noexcept = default;
};

static_assert(std::is_trivial_v<ScoreMove>);

template <typename Ret>
struct AbortFactory;

template <>
struct AbortFactory<AbortStatus> {
    static constexpr AbortStatus err(Abort a) noexcept { return AbortStatus::err(a); }
};

template <typename T>
struct AbortFactory<AbortResultBox<T>> {
    static constexpr AbortResultBox<T> ok(T v) noexcept { return {AbortStatus::ok(), std::move(v)}; }
    static constexpr AbortResultBox<T> err(Abort a) noexcept { return {AbortStatus::err(a), T{}}; }
};

template <typename Ret>
constexpr Ret make_abort(Abort a) noexcept {
    return AbortFactory<Ret>::err(a);
}

#if STELLO_ABORT_EXCEPTIONS

template <typename T>
using AbortResult = T;
using AbortVoid = void;

#define STELLO_TRY_VOID_R(RetValT, expr) (expr)
#define STELLO_TRY_VOID_IN_STATUS(expr) (expr)
#define STELLO_TRY_ASSIGN_R(RetValT, decl, expr) decl = (expr)
#define STELLO_TRY_ASSIGN_EXISTING_R(RetValT, var, expr) var = (expr)

#define STELLO_TRY_VOID_I32(expr) (expr)
#define STELLO_TRY_ASSIGN_I32(decl, expr) decl = (expr)
#define STELLO_TRY_ASSIGN_EXISTING_I32(var, expr) var = (expr)

#define STELLO_RETURN_OK(val) return (val)
#define STELLO_RETURN_OK_R(ValT, val) return (val)
#define STELLO_RETURN_OK_VOID() return
#define STELLO_RETURN_ABORT_IN_STATUS(abort_val) throw ::stello::search::Abort(abort_val)
#define STELLO_RETURN_ABORT_R(RetValT, abort_val) throw ::stello::search::Abort(abort_val)

#else

template <typename T>
using AbortResult = AbortResultBox<T>;
using AbortVoid = AbortStatus;

#define STELLO_ABORT_CAT_INNER(a, b) a##b
#define STELLO_ABORT_CAT(a, b) STELLO_ABORT_CAT_INNER(a, b)

#define STELLO_TRY_VOID_R(RetValT, expr)                                                                  \
    do {                                                                                                  \
        const auto stello_st_ = (expr);                                                                   \
        if (stello_st_.is_err()) {                                                                        \
            return ::stello::search::make_abort<::stello::search::AbortResultBox<RetValT>>(               \
                stello_st_.abort_reason());                                                               \
        }                                                                                                 \
    } while (false)

#define STELLO_TRY_VOID_IN_STATUS(expr)                                                                   \
    do {                                                                                                  \
        const auto stello_st_ = (expr);                                                                   \
        if (stello_st_.is_err()) {                                                                        \
            return ::stello::search::AbortStatus::err(stello_st_.abort_reason());                         \
        }                                                                                                 \
    } while (false)

#define STELLO_TRY_ASSIGN_IN_STATUS(decl, expr)                                                           \
    auto STELLO_ABORT_CAT(stello_res_, __LINE__) = (expr);                                                \
    if (STELLO_ABORT_CAT(stello_res_, __LINE__).is_err()) {                                               \
        return ::stello::search::AbortStatus::err(                                                        \
            STELLO_ABORT_CAT(stello_res_, __LINE__).abort_reason());                                      \
    }                                                                                                     \
    decl = std::move(STELLO_ABORT_CAT(stello_res_, __LINE__).value)

#define STELLO_TRY_ASSIGN_R(RetValT, decl, expr)                                                          \
    auto STELLO_ABORT_CAT(stello_res_, __LINE__) = (expr);                                                \
    if (STELLO_ABORT_CAT(stello_res_, __LINE__).is_err()) {                                               \
        return ::stello::search::make_abort<::stello::search::AbortResultBox<RetValT>>(                   \
            STELLO_ABORT_CAT(stello_res_, __LINE__).abort_reason());                                      \
    }                                                                                                     \
    decl = std::move(STELLO_ABORT_CAT(stello_res_, __LINE__).value)

#define STELLO_TRY_ASSIGN_EXISTING_R(RetValT, var, expr)                                                  \
    do {                                                                                                  \
        auto stello_res_ = (expr);                                                                        \
        if (stello_res_.is_err()) {                                                                       \
            return ::stello::search::make_abort<::stello::search::AbortResultBox<RetValT>>(               \
                stello_res_.abort_reason());                                                              \
        }                                                                                                 \
        (var) = std::move(stello_res_.value);                                                             \
    } while (false)

#define STELLO_TRY_VOID_I32(expr) STELLO_TRY_VOID_R(int32_t, expr)
#define STELLO_TRY_ASSIGN_I32(decl, expr) STELLO_TRY_ASSIGN_R(int32_t, decl, expr)
#define STELLO_TRY_ASSIGN_EXISTING_I32(var, expr) STELLO_TRY_ASSIGN_EXISTING_R(int32_t, var, expr)

#define STELLO_RETURN_OK(val) return ::stello::search::AbortFactory<::stello::search::AbortResultBox<decltype(val)>>::ok(val)
#define STELLO_RETURN_OK_R(ValT, val) return ::stello::search::AbortFactory<::stello::search::AbortResultBox<ValT>>::ok(val)
#define STELLO_RETURN_OK_VOID() return ::stello::search::AbortStatus::ok()
#define STELLO_RETURN_ABORT_IN_STATUS(abort_val) return ::stello::search::AbortStatus::err(abort_val)
#define STELLO_RETURN_ABORT_R(RetValT, abort_val)                                                         \
    return ::stello::search::make_abort<::stello::search::AbortResultBox<RetValT>>(abort_val)

#endif

#if STELLO_ABORT_EXCEPTIONS
#define STELLO_TRY_ASSIGN_IN_STATUS(decl, expr) decl = (expr)
#endif

/// RAII guard mirroring C#'s try/finally around `in_probcut = true`.
struct ProbCutGuard {
    bool& flag;
    explicit ProbCutGuard(bool& f) noexcept : flag(f) { flag = true; }
    ~ProbCutGuard() noexcept { flag = false; }
    ProbCutGuard(const ProbCutGuard&) = delete;
    ProbCutGuard& operator=(const ProbCutGuard&) = delete;
};

} // namespace stello::search
