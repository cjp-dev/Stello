#pragma once

#include "core/bits.hpp"
#include "core/board.hpp"
#include "core/compiler.hpp"
#include "core/square.hpp"
#include "endgame/bits.hpp"
#include "endgame/local.hpp"
#include "search/abort.hpp"
#include "search/limits.hpp"
#include "search/options.hpp"
#include "search/ordering.hpp"
#include "search/pool.hpp"
#include "search/table.hpp"
#include "search/time.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace stello::search {

inline constexpr int32_t INFINITY_SCORE = 32767;
inline constexpr int32_t WIN_SCORE = 32600;
inline constexpr int32_t DEFAULT_PROBCUT_PERCENT = 100;
inline constexpr size_t MAX_MOVES = 64;

inline constexpr int32_t END_INFINITY = 127;
inline constexpr int32_t END_FAST_EMPTIES = 6;
inline constexpr int32_t END_SIMPLE_EMPTIES = 10;
inline constexpr int32_t END_LOCAL_EMPTIES = 13;
inline constexpr int32_t END_ETC_MIN_EMPTIES = 12;
inline constexpr int32_t SOLVE_ORDERING_LOOKS = 4;
inline constexpr int32_t YBWC_MID_MIN_DEPTH = 6;
inline constexpr int32_t YBWC_END_MIN_EMPTIES = 16;

inline constexpr int32_t ORDER_OFFSET = 38;
inline constexpr int32_t ORDER_MOBILITY = 35;
inline constexpr int32_t ORDER_POTENTIAL_MOBILITY = 17;
inline constexpr int32_t ORDER_VALUE = 269;
inline constexpr int32_t ORDER_VALUE_DEEP = 94;
inline constexpr int32_t ORDER_NULL_MOBILITY = 17;
inline constexpr int32_t ORDER_NULL_VALUE = 7;
inline constexpr int32_t ORDER_NULL_VALUE_DEEP = 25;
inline constexpr int32_t ORDER_LOCAL_MOBILITY = 40;
inline constexpr int32_t ORDER_SIMPLE_MOBILITY = 18;
inline constexpr int32_t ORDER_SIMPLE_PARITY = 17;
inline constexpr int32_t ORDER_HASH_MOVE = 10'000'000;

/// t * sigma in 1/100 disc by disc count (0-64) and depth (1-60).
using ProbCutErrors = std::array<std::array<int32_t, 61>, 65>;

using Clock = Duration (*)();
Duration system_clock();

/// What can stop a running search; read at every abort check (every 1024 nodes) and at the start.
class StopSignal {
public:
    virtual ~StopSignal() = default;
    virtual bool is_cancelled() const noexcept = 0;
    virtual bool is_move_now() const noexcept = 0;
};

class SearchSignals final : public StopSignal {
public:
    SearchSignals() noexcept = default;

    void cancel() noexcept { cancel_.store(true, std::memory_order_relaxed); }
    void move_now() noexcept { move_now_.store(true, std::memory_order_relaxed); }
    void reset() noexcept {
        cancel_.store(false, std::memory_order_relaxed);
        move_now_.store(false, std::memory_order_relaxed);
    }

    bool is_cancelled() const noexcept override { return cancel_.load(std::memory_order_relaxed); }
    bool is_move_now() const noexcept override { return move_now_.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> cancel_{false};
    std::atomic<bool> move_now_{false};
};

class NoStop final : public StopSignal {
public:
    bool is_cancelled() const noexcept override { return false; }
    bool is_move_now() const noexcept override { return false; }
};

inline const NoStop& no_stop() noexcept {
    static const NoStop instance;
    return instance;
}

enum class SearchError : uint8_t {
    Cancelled = 0,
    NoLegalOnlyMove = 1,
};

inline constexpr std::string_view search_error_message(SearchError err) noexcept {
    switch (err) {
        case SearchError::Cancelled: return "The search was cancelled.";
        case SearchError::NoLegalOnlyMove: return "None of the moves to search is legal.";
    }
    return "Search error.";
}

struct SearchOutcome {
    bool ok{false};
    SearchResult result{};
    SearchError error{SearchError::Cancelled};

    constexpr explicit operator bool() const noexcept { return ok; }
};

struct EndgameState {
    endgame::LocalTable local;

    void clear() noexcept { local.clear(); }
};

struct ThreadState {
    EndgameState endgame;
    MoveOrdering ordering;
};

struct EndMove {
    uint32_t square{0};
    uint64_t flips{0};
    int32_t value{0};
    uint64_t replies{endgame::bits::LEGAL_UNDEFINED};
};

struct Best {
    int32_t mv{-1};
    int32_t score{0};
    ScoreKind kind{ScoreKind::None};
    int32_t depth{0};
};

struct SearchContext {
    const eval::EvalWeights* pattern{nullptr};
    const ProbCutErrors* probcut_errors{nullptr};
    const TranspositionTable* midgame_table{nullptr};
    const TranspositionTable* endgame_table{nullptr};
    Clock clock{system_clock};
    std::optional<int32_t> soft_time_percent;
    bool ybwc{true};
    uint32_t root_empties{64};

    static SearchContext create(
        const SearchOptions& options,
        const ProbCutErrors* probcut_errors,
        const TranspositionTable& midgame_table,
        const TranspositionTable& endgame_table,
        Clock clock,
        uint32_t root_empties
    );
};

using ProgressCallback = std::function<void(const SearchInfo&)>;
using IterationDoneCallback = std::function<void(int32_t, int32_t)>;

struct SplitPoint;
struct TaskSetup;
class Searcher;

/// Shared pool handle passed from the main search to workers and split nodes.
struct Helpers {
    const ThreadPool<ThreadState>* pool{nullptr};
    SearchContext context{};
    size_t count{0};
    std::atomic<uint64_t>* nodes{nullptr};
    std::atomic<uint64_t>* evaluations{nullptr};

    Searcher searcher(ThreadState& state, const StopSignal& stop_signal) const;
    void finish(Searcher& helper) const noexcept;
};

class Searcher {
public:
    Searcher(const SearchContext& context, ThreadState& thread, const StopSignal& stop);

    SearchOutcome run(
        const Board& board,
        Player player,
        const SearchLimits& limits,
        std::optional<uint64_t> only_moves = std::nullopt
    );

    // Search routines (midgame in search/engine.cpp, endgame in endgame/search.cpp & endgame/last.cpp)
    AbortVoid iterate(
        const Board& board,
        Player player,
        const SearchLimits& limits,
        std::span<int32_t> root_moves,
        Best& best
    );

    AbortResult<bool> midgame_iterations(
        const Board& board,
        Player player,
        const SearchLimits& limits,
        time::TimeBudget budget,
        std::span<int32_t> root_moves,
        Best& best,
        std::atomic<int32_t>* main_look
    );

    AbortVoid search_root(
        const Board& board,
        Player player,
        int32_t look,
        std::span<int32_t> root_moves,
        Best& best
    );

    AbortResult<ScoreMove> search(
        const Board& board,
        Player player,
        int32_t look,
        int32_t ply,
        int32_t previous_move,
        int32_t alpha,
        int32_t beta
    );

    AbortResult<ScoreMove> solve_root(
        const Board& board,
        Player player,
        int32_t alpha,
        int32_t beta,
        ScoreKind kind,
        std::span<int32_t> root_moves,
        Best& best
    );

    AbortResult<int32_t> solve_child(uint64_t own, uint64_t opponent, int32_t alpha, int32_t beta, int32_t empties);
    AbortResult<int32_t> end_null(uint64_t own, uint64_t opponent, int32_t alpha, int32_t empties, uint64_t legal);
    AbortResult<int32_t> end_window(uint64_t own, uint64_t opponent, int32_t alpha, int32_t beta, int32_t empties, bool passed);
    AbortResult<int32_t> solve_shallow(uint64_t own, uint64_t opponent, int32_t alpha, int32_t beta, int32_t empties, bool passed);
    AbortResult<int32_t> end_deep(uint64_t own, uint64_t opponent, int32_t alpha, int32_t empties, bool passed, uint64_t legal);
    AbortResult<int32_t> ordering_value(uint64_t own, uint64_t opponent, int32_t depth, int32_t alpha, int32_t beta);
    AbortResult<int32_t> last4(uint64_t own, uint64_t opponent, int32_t alpha);
    AbortResult<int32_t> last3(uint64_t own, uint64_t opponent, int32_t alpha, uint32_t p0, uint32_t p1, uint32_t p2, uint64_t empties4);
    AbortResult<int32_t> last2(uint64_t own, uint64_t opponent, int32_t alpha, uint32_t p0, uint32_t p1);
    AbortResult<int32_t> last1(uint64_t own, int32_t alpha, uint32_t square);

    STELLO_FORCEINLINE AbortVoid count_node() {
        nodes += 1;
        if (nodes >= next_abort_check) [[unlikely]] {
            next_abort_check = nodes + 1024;
            return check_abort();
        }
        STELLO_RETURN_OK_VOID();
    }

    STELLO_NOINLINE AbortVoid check_abort();
    void flush_nodes() noexcept;

    bool may_split() const noexcept { return helpers.has_value() && ybwc; }

    template <typename ChildT>
    AbortVoid split(
        std::span<const ChildT> children,
        int32_t alpha,
        int32_t beta,
        int32_t& best,
        int32_t& best_move
    );

    void split_task(const Helpers& h, const SplitPoint* point, const TaskSetup& setup) noexcept;

    // Fields readable by search modules and unit tests
    const eval::EvalWeights* pattern{nullptr};
    const ProbCutErrors* probcut_errors{nullptr};
    const TranspositionTable* midgame_table{nullptr};
    const TranspositionTable* endgame_table{nullptr};
    EndgameState* endgame{nullptr};
    MoveOrdering* ordering{nullptr};
    ProgressCallback progress;
    IterationDoneCallback* iteration_done{nullptr};
    const StopSignal* stop{nullptr};
    Clock clock{system_clock};
    Duration start{Duration::zero()};
    std::optional<int32_t> soft_time_percent;
    bool ybwc{true};
    uint32_t root_empties{64};
    const SplitPoint* split_point{nullptr};
    std::optional<Helpers> helpers;
    std::atomic<uint64_t>* node_sink{nullptr};
    std::atomic<uint64_t>* node_source{nullptr};
    uint64_t flushed_nodes{0};

    uint64_t nodes{0};
    uint64_t evaluations{0};
    uint64_t next_abort_check{1024};
    int64_t hard_limit_ms{(std::numeric_limits<int64_t>::max)()};
    int32_t hash_get_height{0};
    int32_t hash_put_height{0};
    bool in_probcut{false};
    int64_t last_heartbeat_ms{0};
    int32_t report_depth{0};
    int32_t report_move{-1};
    int32_t report_best_move{-1};
    int32_t report_score{0};
    ScoreKind report_kind{ScoreKind::None};

private:
    AbortVoid order_by_evaluation(std::span<int32_t> list, int32_t hash_move, const Board& board, Player player);
    AbortResult<std::optional<int32_t>> try_probcut(
        const Board& board,
        Player player,
        int32_t look,
        int32_t ply,
        int32_t previous_move,
        int32_t alpha,
        int32_t beta
    );
    AbortResult<std::optional<int32_t>> probcut_searches(
        const Board& board,
        Player player,
        int32_t shallow_look,
        int32_t ply,
        int32_t previous_move,
        int32_t alpha,
        int32_t beta,
        int32_t error,
        int32_t error0,
        int32_t static0
    );
    AbortResult<int32_t> evaluate_leaf(const Board& board, Player player);
    AbortResult<bool> lazy_smp_midgame(
        const Helpers& smp_helpers,
        const Board& board,
        Player player,
        const SearchLimits& limits,
        time::TimeBudget budget,
        std::span<int32_t> root_moves,
        Best& best
    );
    AbortResult<ScoreMove> solve_iteration(
        const Board& board,
        Player player,
        int32_t alpha,
        int32_t beta,
        ScoreKind kind,
        std::span<int32_t> root_moves,
        Best& best
    );

    AbortResult<int32_t> end_local(uint64_t own, uint64_t opponent, int32_t alpha, bool passed, uint64_t legal, int32_t empties);
    AbortResult<int32_t> end_simple(uint64_t own, uint64_t opponent, int32_t alpha, bool passed, uint64_t legal, int32_t empties);
    AbortResult<int32_t> end_fast(uint64_t own, uint64_t opponent, int32_t alpha, bool passed, int32_t empties);
    AbortResult<int32_t> solve_last2(uint64_t own, uint64_t opponent, int32_t alpha, int32_t beta, uint64_t empty, bool passed);
    int32_t solve_last(uint64_t own, uint64_t opponent, uint64_t empty) noexcept;

    bool stop_iterating(const SearchLimits& limits, time::TimeBudget budget, int32_t next_look) const noexcept;
    void report(int32_t depth, int32_t mv, int32_t best_move, int32_t score, ScoreKind kind);
    Duration elapsed() const;
    int64_t elapsed_ms() const;
    SearchResult make_result(const Best& best) const;
};

class SearchEngine {
public:
    explicit SearchEngine(uint32_t hash_bits, SearchOptions options = {});

    uint32_t hash_bits() const noexcept { return hash_bits_; }
    const SearchOptions& options() const noexcept { return options_; }

    void clear_hash();
    void set_ybwc(bool ybwc) noexcept { options_.ybwc = ybwc; }
    void set_clock(Clock clock) noexcept { clock_ = clock; }
    void set_spawner(Spawner spawner) noexcept { spawner_ = spawner; }
    void set_iteration_done(IterationDoneCallback callback) { iteration_done_ = std::move(callback); }

    SearchOutcome search(
        const Board& board,
        Player player,
        const SearchLimits& limits,
        ProgressCallback progress = {},
        const StopSignal& stop = no_stop(),
        std::optional<uint64_t> only_moves = std::nullopt
    );

    SearchResult search_simple(const Board& board, Player player, const SearchLimits& limits);

    /// Creates a `Searcher` sharing this engine's tables and main thread state (for unit tests).
    Searcher searcher(
        ProgressCallback progress,
        const StopSignal& stop,
        uint32_t root_empties
    );

    const TranspositionTable& midgame_table() const noexcept { return midgame_table_; }
    const TranspositionTable& endgame_table() const noexcept { return endgame_table_; }

private:
    uint32_t hash_bits_;
    SearchOptions options_;
    std::unique_ptr<ProbCutErrors> probcut_errors_;
    TranspositionTable midgame_table_;
    TranspositionTable endgame_table_;
    ThreadState main_;
    uint32_t pool_threads_{0};
    std::unique_ptr<ThreadPool<ThreadState>> pool_;
    Spawner spawner_{&std_spawner};
    Clock clock_{system_clock};
    IterationDoneCallback iteration_done_;
};

/// The final disc difference for the side owning `own`, the empty squares going to the winner.
inline int32_t final_score(uint64_t own, uint64_t opponent) noexcept {
    const int32_t own_count = static_cast<int32_t>(std::popcount(own));
    const int32_t opponent_count = static_cast<int32_t>(std::popcount(opponent));
    const int32_t empties = 64 - own_count - opponent_count;
    const int32_t difference = own_count - opponent_count;
    if (difference > 0) {
        return difference + empties;
    }
    if (difference < 0) {
        return difference - empties;
    }
    return 0;
}

inline int32_t game_over_score(uint64_t own, uint64_t opponent) noexcept {
    const int32_t difference = final_score(own, opponent);
    if (difference > 0) {
        return difference + WIN_SCORE;
    }
    if (difference < 0) {
        return difference - WIN_SCORE;
    }
    return 0;
}

inline size_t fill_moves(std::span<int32_t> list, uint64_t moves) noexcept {
    size_t count = 0;
    while (moves != 0) {
        list[count] = static_cast<int32_t>(std::countr_zero(moves));
        ++count;
        moves &= (moves - 1);
    }
    return count;
}

namespace detail {

int32_t probcut_shallow_depth(int32_t depth) noexcept;
std::unique_ptr<ProbCutErrors> make_probcut_errors(double t);
void move_to_front(std::span<int32_t> moves, std::span<const int32_t> improvements) noexcept;
int32_t last_flip_count_for_test(uint64_t own, uint32_t square) noexcept;
bool fill_end_moves_for_test(uint64_t own, uint64_t opponent, uint64_t legal, std::span<EndMove> moves) noexcept;
size_t undone_first_for_test(std::span<EndMove> moves, uint64_t done) noexcept;
void swap_next_best_for_test(std::span<EndMove> moves, size_t start) noexcept;

} // namespace detail

} // namespace stello::search
