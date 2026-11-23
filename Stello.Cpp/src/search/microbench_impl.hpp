#pragma once

#include "core/bits.hpp"
#include "core/board.hpp"
#include "core/compiler.hpp"
#include "endgame/bits.hpp"
#include "endgame/last_impl.hpp"
#include "endgame/stable.hpp"
#include "eval/eval.hpp"
#include "eval/patterns.hpp"
#include "eval/weights.hpp"
#include "search/abort.hpp"
#include "search/kernel.hpp"
#include "search/microbench.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86)) && !defined(__EMSCRIPTEN__)
#include <intrin.h>
#define STELLO_MICROBENCH_RDTSC 1
#elif (defined(__x86_64__) || defined(__i386__)) && !defined(__EMSCRIPTEN__)
#include <x86intrin.h>
#define STELLO_MICROBENCH_RDTSC 2
#else
#define STELLO_MICROBENCH_RDTSC 0
#endif

namespace stello::search::detail::microbench {

inline uint64_t read_cycles() noexcept {
#if STELLO_MICROBENCH_RDTSC == 1 || STELLO_MICROBENCH_RDTSC == 2
    return __rdtsc();
#else
    return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

template <class T>
STELLO_FORCEINLINE void mb_sink(T& value) noexcept {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : "+r,m"(value) : : "memory");
#elif defined(_MSC_VER)
    static volatile uint64_t sink_guard = 0;
    sink_guard = static_cast<uint64_t>(value);
    _ReadWriteBarrier();
#else
    (void)value;
#endif
}

STELLO_FORCEINLINE int32_t unwrap_i32(AbortResult<int32_t> r) noexcept {
#if STELLO_ABORT_EXCEPTIONS
    return r;
#else
    return r.value;
#endif
}

struct BoardPair {
    uint64_t own{0};
    uint64_t opp{0};
};

struct alignas(32) EvalSample {
    eval::PatternState state{};
    alignas(32) std::array<uint16_t, eval::MAX_PATTERN_FEATURES> parent_base{};
    uint64_t own{0};
    uint64_t opp{0};
    uint64_t flips{0};
    uint32_t square{0};
    Player player{Player::Black};
    const int16_t* weights{nullptr};
};

STELLO_NOINLINE inline std::array<BoardPair, 16> make_classic_ring() noexcept {
    // Classic Edax bench.c board: border O's, 6x6 interior X's.
    constexpr uint64_t kBorder = 0xFF81'8181'8181'81FFULL;
    constexpr uint64_t kInner6 = 0x007E'7E7E'7E7E'7E00ULL;
    constexpr std::array<uint32_t, 16> kSaltBits = {
        18U, 19U, 20U, 21U, 26U, 27U, 28U, 29U,
        34U, 35U, 36U, 37U, 42U, 43U, 44U, 45U,
    };
    std::array<BoardPair, 16> ring{};
    for (size_t i = 0; i < ring.size(); ++i) {
        const uint64_t hole = uint64_t{1} << kSaltBits[i];
        ring[i] = BoardPair{kBorder, kInner6 & ~hole};
    }
    return ring;
}

STELLO_NOINLINE inline std::vector<BoardPair> make_midgame_boards(size_t count) {
    std::vector<BoardPair> boards;
    boards.reserve(count);
    uint64_t rng = 0x9E37'79B9'7F4A'7C15ULL;
    auto next_rng = [&]() noexcept -> uint64_t {
        rng += 0x9E37'79B9'7F4A'7C15ULL;
        uint64_t z = rng;
        z = (z ^ (z >> 30U)) * 0xBF58'476D'1CE4'E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D0'49BB'1331'11EBULL;
        return z ^ (z >> 31U);
    };

    while (boards.size() < count) {
        Board board = Board::initial();
        Player player = Player::Black;
        const uint32_t target_plies = 16U + static_cast<uint32_t>(next_rng() % 30ULL);
        for (uint32_t ply = 0; ply < target_plies; ++ply) {
            uint64_t moves = board.legal_moves(player);
            if (moves == 0) {
                player = opponent(player);
                moves = board.legal_moves(player);
                if (moves == 0) {
                    break;
                }
            }
            const uint32_t move_count = static_cast<uint32_t>(std::popcount(moves));
            uint32_t pick = static_cast<uint32_t>(next_rng() % move_count);
            uint64_t rem = moves;
            uint32_t sq = 0;
            for (uint32_t k = 0; k <= pick; ++k) {
                sq = static_cast<uint32_t>(std::countr_zero(rem));
                rem &= rem - 1ULL;
            }
            const uint64_t f = board.flips(player, Square(static_cast<uint8_t>(sq)));
            board = board.play_flips(player, sq, f);
            player = opponent(player);
        }
        if (board.legal_moves(player) != 0) {
            boards.push_back(BoardPair{board.discs(player), board.discs(opponent(player))});
        }
    }
    return boards;
}

STELLO_NOINLINE inline std::vector<EvalSample> make_eval_samples(size_t count) {
    const eval::EvalWeights& ew = eval::EvalWeights::built_in();
    const eval::PatternSet& set = ew.set();
    const std::vector<BoardPair> raw = make_midgame_boards(count);
    std::vector<EvalSample> out(count);
    for (size_t i = 0; i < count; ++i) {
        EvalSample& s = out[i];
        s.player = (i & 1U) == 0U ? Player::Black : Player::White;
        s.own = raw[i].own;
        s.opp = raw[i].opp;
        const Board board = Board::from_own(s.player, s.own, s.opp);
        s.state = set.state_of(board.black(), board.white());
        set.build_base(s.state, s.player == Player::White, s.parent_base);
        const uint64_t legal = bits::legal_moves(s.own, s.opp);
        s.square = static_cast<uint32_t>(std::countr_zero(legal));
        s.flips = bits::flips(s.own, s.opp, s.square);
        const uint32_t discs = static_cast<uint32_t>(std::popcount(s.own | s.opp)) + 1U;
        const size_t stage = ew.stage_of((std::min)(discs, 64U));
        s.weights = ew.stage_weights(stage).data();
    }
    return out;
}

template <typename Fn>
MicrobenchStats measure_64(
    std::string_view name,
    uint64_t warmup,
    uint64_t repeats,
    bool keep_per_sample,
    Fn&& fn
) {
    uint64_t sum = 0;

    // Warmup and overhead measurement using an opaque accumulator mix.
    for (uint64_t i = 0; i < warmup; ++i) {
        uint64_t x = i * 0x9E37'79B9'7F4A'7C15ULL;
        mb_sink(x);
        sum ^= x;
    }

    uint64_t best_overhead = (std::numeric_limits<uint64_t>::max)();
    for (int trial = 0; trial < 3; ++trial) {
        const uint64_t t0 = read_cycles();
        for (uint64_t i = 0; i < repeats; ++i) {
            uint64_t x = i;
            mb_sink(x);
            sum += x;
        }
        const uint64_t t1 = read_cycles();
        best_overhead = (std::min)(best_overhead, t1 - t0);
    }

    MicrobenchStats stats{};
    stats.name = name;
    stats.min_cycles = (std::numeric_limits<double>::max)();
    stats.max_cycles = 0.0;
    if (keep_per_sample) {
        stats.per_sample.reserve(64);
    }

    double sum_t = 0.0;
    double sum_t2 = 0.0;

    for (uint32_t s = 0; s < 64U; ++s) {
        for (uint64_t i = 0; i < warmup; ++i) {
            sum += static_cast<uint64_t>(fn(s, i));
        }

        const uint64_t t0 = read_cycles();
        for (uint64_t i = 0; i < repeats; ++i) {
            sum += static_cast<uint64_t>(fn(s, i));
        }
        const uint64_t t1 = read_cycles();
        const uint64_t delta = t1 - t0;
        const uint64_t net = (delta > best_overhead) ? (delta - best_overhead) : 0ULL;
        const double t = static_cast<double>(net) / static_cast<double>(repeats);

        sum_t += t;
        sum_t2 += t * t;
        stats.min_cycles = (std::min)(stats.min_cycles, t);
        stats.max_cycles = (std::max)(stats.max_cycles, t);
        if (keep_per_sample) {
            stats.per_sample.push_back(t);
        }
    }

    mb_sink(sum);
    stats.mean_cycles = sum_t / 64.0;
    const double var = (std::max)(0.0, sum_t2 / 64.0 - stats.mean_cycles * stats.mean_cycles);
    stats.stddev_cycles = std::sqrt(var);
    stats.checksum = sum;
    return stats;
}

template <bool UseAvx2>
std::vector<MicrobenchStats> run_kernel(const MicrobenchOptions& opts) {
    endgame::stable::init();
    const eval::EvalWeights& ew = eval::EvalWeights::built_in();
    const eval::PatternSet& set = ew.set();

    const std::array<BoardPair, 16> classic = make_classic_ring();
    const std::vector<BoardPair> midboards = make_midgame_boards(64);
    const std::vector<EvalSample> eval_samples = make_eval_samples(16);

    auto want = [&](std::string_view name) noexcept {
        return opts.only.empty() || opts.only == name;
    };

    std::vector<MicrobenchStats> results;

    if (want("flips")) {
        results.push_back(measure_64("flips", opts.warmup, opts.repeats, opts.verbose,
            [&](uint32_t sq, uint64_t i) noexcept -> uint64_t {
                const BoardPair& b = classic[static_cast<size_t>(i & 15U)];
                const uint64_t bit = uint64_t{1} << sq;
                return Kernel<UseAvx2>::flips(b.own & ~bit, b.opp & ~bit, sq);
            }));
    }

    if (want("last_flip_count")) {
        results.push_back(measure_64("last_flip_count", opts.warmup, opts.repeats, opts.verbose,
            [&](uint32_t sq, uint64_t i) noexcept -> uint64_t {
                const BoardPair& b = classic[static_cast<size_t>(i & 15U)];
                const uint64_t bit = uint64_t{1} << sq;
                return static_cast<uint64_t>(detail::last::last_flip_count(b.own & ~bit, sq));
            }));
    }

    if (want("solve_1")) {
        TranspositionTable mid_tt(10);
        TranspositionTable end_tt(10);
        auto thread = std::make_unique<ThreadState>();
        const SearchContext ctx = SearchContext::create(
            SearchOptions{}, nullptr, nullptr, mid_tt, end_tt, system_clock, 1);
        Searcher searcher(ctx, *thread, no_stop());
        searcher.next_abort_check = (std::numeric_limits<uint64_t>::max)();
        results.push_back(measure_64("solve_1", opts.warmup, opts.repeats, opts.verbose,
            [&](uint32_t sq, uint64_t i) noexcept -> uint64_t {
                const BoardPair& b = classic[static_cast<size_t>(i & 15U)];
                const uint64_t bit = uint64_t{1} << sq;
                return static_cast<uint64_t>(static_cast<int64_t>(
                    unwrap_i32(Kernel<UseAvx2>::last1(searcher, b.own & ~bit, 63, sq))));
            }));
    }

    if (want("legal_moves")) {
        results.push_back(measure_64("legal_moves", opts.warmup, opts.repeats, opts.verbose,
            [&](uint32_t s, uint64_t i) noexcept -> uint64_t {
                const BoardPair& b = midboards[static_cast<size_t>((s + i) & 15U)];
                return Kernel<UseAvx2>::legal_moves(b.own, b.opp);
            }));
    }

    if (want("potential_mobility")) {
        results.push_back(measure_64("potential_mobility", opts.warmup, opts.repeats, opts.verbose,
            [&](uint32_t s, uint64_t i) noexcept -> uint64_t {
                const BoardPair& b = midboards[static_cast<size_t>((s + i) & 15U)];
                return static_cast<uint64_t>(static_cast<int64_t>(bits::potential_mobility(b.own, b.opp)));
            }));
    }

    if (want("legal_and_potential")) {
        results.push_back(measure_64("legal_and_potential", opts.warmup, opts.repeats, opts.verbose,
            [&](uint32_t s, uint64_t i) noexcept -> uint64_t {
                const BoardPair& b = midboards[static_cast<size_t>((s + i) & 15U)];
                return Kernel<UseAvx2>::legal_moves(b.own, b.opp)
                     + static_cast<uint64_t>(static_cast<int64_t>(bits::potential_mobility(b.own, b.opp)));
            }));
    }

    if (want("stable_of")) {
        results.push_back(measure_64("stable_of", opts.warmup, opts.repeats, opts.verbose,
            [&](uint32_t s, uint64_t i) noexcept -> uint64_t {
                const BoardPair& b = midboards[static_cast<size_t>((s + i) & 63U)];
                return endgame::stable::of(b.own, b.opp);
            }));
    }

    if (want("eval_sum")) {
        results.push_back(measure_64("eval_sum", opts.warmup, opts.repeats, opts.verbose,
            [&](uint32_t s, uint64_t i) noexcept -> uint64_t {
                const EvalSample& sample = eval_samples[static_cast<size_t>((s + i) & 15U)];
                return static_cast<uint64_t>(static_cast<int64_t>(
                    Kernel<UseAvx2>::sum_eval_state(set, sample.state, sample.player, sample.weights)));
            }));
    }

    if (want("eval_after_move")) {
        results.push_back(measure_64("eval_after_move", opts.warmup, opts.repeats, opts.verbose,
            [&](uint32_t s, uint64_t i) noexcept -> uint64_t {
                const EvalSample& sample = eval_samples[static_cast<size_t>((s + i) & 15U)];
                return static_cast<uint64_t>(static_cast<int64_t>(
                    Kernel<UseAvx2>::sum_eval_after_move(set, sample.parent_base, sample.square, sample.flips, sample.weights)));
            }));
    }

    if (want("eval_advance")) {
        eval::PatternState next_state{};
        results.push_back(measure_64("eval_advance", opts.warmup, opts.repeats, opts.verbose,
            [&](uint32_t s, uint64_t i) noexcept -> uint64_t {
                const EvalSample& sample = eval_samples[static_cast<size_t>((s + i) & 15U)];
                Kernel<UseAvx2>::advance_eval_state(set, next_state, sample.state, sample.player, sample.square, sample.flips);
                return static_cast<uint64_t>(next_state.black[0]) + static_cast<uint64_t>(next_state.white[0]);
            }));
    }

    return results;
}

} // namespace stello::search::detail::microbench
