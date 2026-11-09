#include "cli/bench.hpp"
#include "cli/options.hpp"
#include "core/board.hpp"
#include "core/cpu.hpp"
#include "core/version.hpp"
#include "eval/eval.hpp"
#include "eval/weights.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace {

constexpr const char* kUsage =
    "Usage: stello-cpp <command> [options]\n\n"
    "  version                   The engine version.\n"
    "  perft [--depth 8]         Perft from the start position to every depth up to --depth (a pass counts as one move),\n"
    "                            with the time of each depth.\n"
    "  eval-split [<folder>] [--max-per-discs 20000|all] [--eval <file>] [--cpu auto|portable]\n"
    "                            Stello 2's evaluation over the trainer's test split (test-dNN.bin).\n"
    "  solve-bench [--positions <file>] [--from 40] [--to 44] [--passes 1] [--hash-bits 19] [--eval <file>]\n"
    "              [--out <file.json>] [--cpu auto|portable] [--threads 1] [--ybwc on|off]\n"
    "                            Solve benchmark positions and check exact scores/moves.\n"
    "  mid-bench [--positions <file>] [--depth 10] [--passes 3] [--hash-bits 19] [--eval <file>]\n"
    "            [--selective on|off] [--probcut-pct N] [--out <file.json>] [--cpu auto|portable] [--threads 1] [--ybwc on|off]\n"
    "                            Search midgame benchmark positions to a fixed depth.\n";

constexpr uint64_t kFnvOffset = 14'695'981'039'346'656'037ULL;
constexpr uint64_t kFnvPrime = 1'099'511'628'211ULL;

inline uint64_t fnv_i32(uint64_t hash, int32_t value) noexcept {
    const uint32_t u = static_cast<uint32_t>(value);
    for (int i = 0; i < 4; ++i) {
        hash = (hash ^ ((u >> (8 * i)) & 0xFFU)) * kFnvPrime;
    }
    return hash;
}

template <class T>
inline void do_not_optimize(T& value) noexcept {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : "+r,m"(value) : : "memory");
#elif defined(_MSC_VER)
    static volatile T* volatile sink_ptr = &value;
    (void)sink_ptr;
    _ReadWriteBarrier();
#endif
}

std::optional<uint64_t> parse_u64(std::string_view text) noexcept {
    if (text.empty()) {
        return std::nullopt;
    }
    uint64_t v = 0;
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, v);
    if (ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return v;
}

bool apply_cpu_option(std::optional<std::string_view> cpu_opt, std::string& error_out) {
    if (!cpu_opt.has_value() || *cpu_opt == "auto") {
        stello::cpu::set_portable_only(false);
        return true;
    }
    if (*cpu_opt == "portable") {
        stello::cpu::set_portable_only(true);
        return true;
    }
    error_out = "--cpu must be auto or portable.";
    return false;
}

int cmd_perft(std::span<const std::string_view> args, std::string& error_out) {
    static constexpr std::array<std::string_view, 1> kAllowed = {"depth"};
    const auto opts = stello::cli::Options::parse(args, kAllowed, error_out);
    if (!opts) {
        return 1;
    }

    uint32_t depth = 8;
    if (const auto d_str = opts->text("depth")) {
        const auto parsed = parse_u64(*d_str);
        if (!parsed || *parsed < 1 || *parsed > 14) {
            error_out = "--depth must be 1-14.";
            return 1;
        }
        depth = static_cast<uint32_t>(*parsed);
    }

    for (uint32_t d = 1; d <= depth; ++d) {
        stello::Board start = stello::Board::initial();
        do_not_optimize(start);
        const auto t0 = std::chrono::steady_clock::now();
        uint64_t leaves = stello::perft(start, stello::Player::Black, d);
        do_not_optimize(leaves);
        const auto t1 = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::printf("perft %u %llu %.1f ms\n", d, static_cast<unsigned long long>(leaves), ms);
    }
    return 0;
}

int cmd_eval_split(std::span<const std::string_view> args, std::string& error_out) {
    static constexpr std::array<std::string_view, 3> kAllowed = {"max-per-discs", "eval", "cpu"};
    const auto opts = stello::cli::Options::parse(args, kAllowed, error_out);
    if (!opts) {
        return 1;
    }
    if (!apply_cpu_option(opts->text("cpu"), error_out)) {
        return 1;
    }

    std::filesystem::path folder;
    if (opts->positional.empty()) {
        const auto root = stello::cli::repository_folder();
        if (!root) {
            error_out = "No folder given and no repository found.";
            return 1;
        }
        folder = *root / "Traning data" / "stello-bin";
    } else if (opts->positional.size() == 1) {
        folder = opts->positional[0];
    } else {
        error_out = kUsage;
        return 1;
    }

    uint64_t max_per_discs = 20'000;
    if (const auto m_str = opts->text("max-per-discs")) {
        if (*m_str == "all") {
            max_per_discs = (std::numeric_limits<uint64_t>::max)();
        } else {
            const auto parsed = parse_u64(*m_str);
            if (!parsed || *parsed < 1) {
                error_out = "--max-per-discs must be a whole number from 1, or all.";
                return 1;
            }
            max_per_discs = *parsed;
        }
    }

    std::optional<stello::eval::EvalWeights> loaded;
    const stello::eval::EvalWeights* weights = nullptr;
    if (const auto eval_path = opts->text("eval")) {
        stello::eval::EvalError err;
        loaded = stello::eval::EvalWeights::load(std::filesystem::path(*eval_path), &err);
        if (!loaded) {
            error_out = std::string(*eval_path) + ": " + err.message();
            return 1;
        }
        weights = &*loaded;
    } else {
        weights = &stello::eval::EvalWeights::built_in();
    }

    struct Sample {
        uint64_t own;
        uint64_t opponent;
    };
    std::vector<Sample> samples;
    for (uint32_t discs = 4; discs <= 64; ++discs) {
        char filename[32];
        std::snprintf(filename, sizeof(filename), "test-d%02u.bin", discs);
        const auto path = folder / filename;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            continue;
        }
        const auto size = file.tellg();
        if (size <= 0) {
            continue;
        }
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) {
            continue;
        }
        const uint64_t count = static_cast<uint64_t>(bytes.size()) / 17ULL;
        const uint64_t take = count < max_per_discs ? count : max_per_discs;
        for (uint64_t i = 0; i < take; ++i) {
            const uint64_t source = (take == count) ? i : (i * count / take);
            const size_t off = static_cast<size_t>(source * 17ULL);
            uint64_t own = 0;
            uint64_t opp = 0;
            for (int b = 0; b < 8; ++b) {
                own |= static_cast<uint64_t>(bytes[off + b]) << (8 * b);
                opp |= static_cast<uint64_t>(bytes[off + 8 + b]) << (8 * b);
            }
            samples.push_back(Sample{own, opp});
        }
    }

    if (samples.empty()) {
        error_out = folder.string() + " holds no test positions (test-dNN.bin).";
        return 1;
    }

    uint64_t eval_hash = kFnvOffset;
    uint64_t score_hash = kFnvOffset;
    for (const auto& s : samples) {
        eval_hash = fnv_i32(eval_hash, weights->evaluate(s.own, s.opponent));
        score_hash = fnv_i32(score_hash, stello::eval::pattern_score(*weights, s.own, s.opponent));
    }

    std::printf("positions %zu\n", samples.size());
    std::printf("eval-fnv %llx\n", static_cast<unsigned long long>(eval_hash));
    std::printf("score-fnv %llx\n", static_cast<unsigned long long>(score_hash));

    double best_eval_s = (std::numeric_limits<double>::max)();
    int64_t sink = 0;
    for (int pass = 0; pass < 3; ++pass) {
        const auto t0 = std::chrono::steady_clock::now();
        for (auto s : samples) {
            uint64_t own = s.own;
            do_not_optimize(own);
            sink += static_cast<int64_t>(weights->evaluate(own, s.opponent));
        }
        const auto t1 = std::chrono::steady_clock::now();
        const double sec = std::chrono::duration<double>(t1 - t0).count();
        if (sec < best_eval_s) best_eval_s = sec;
    }
    do_not_optimize(sink);
    const double speed_ms = static_cast<double>(samples.size()) / best_eval_s / 1e6;
    std::printf("eval-speed %.2f M/s (%.0f ms per pass, sum %lld)\n",
                speed_ms, best_eval_s * 1000.0, static_cast<long long>(sink));

    double best_perft_s = (std::numeric_limits<double>::max)();
    uint64_t leaves = 0;
    for (int pass = 0; pass < 3; ++pass) {
        stello::Board start = stello::Board::initial();
        do_not_optimize(start);
        const auto t0 = std::chrono::steady_clock::now();
        leaves = stello::perft(start, stello::Player::Black, 8);
        do_not_optimize(leaves);
        const auto t1 = std::chrono::steady_clock::now();
        const double sec = std::chrono::duration<double>(t1 - t0).count();
        if (sec < best_perft_s) best_perft_s = sec;
    }
    std::printf("perft8 %llu in %.1f ms\n", static_cast<unsigned long long>(leaves), best_perft_s * 1000.0);
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (args.empty()) {
        std::cerr << kUsage;
        return 1;
    }

    const std::string_view cmd = args[0];
    const auto rest = std::span<const std::string_view>(args).subspan(1);
    std::string error;
    int rc = 1;

    if (cmd == "version") {
        std::cout << "stello-cpp " << stello::kEngineVersion
                  << " (" << stello::kEngineVersionCode << ")\n";
        return 0;
    } else if (cmd == "perft") {
        rc = cmd_perft(rest, error);
    } else if (cmd == "eval-split") {
        rc = cmd_eval_split(rest, error);
    } else if (cmd == "solve-bench") {
        rc = stello::cli::cmd_solve_bench(rest, error);
    } else if (cmd == "mid-bench") {
        rc = stello::cli::cmd_mid_bench(rest, error);
    } else {
        std::cerr << kUsage;
        return 1;
    }

    if (rc != 0 && !error.empty()) {
        std::cerr << error << "\n";
    }
    return rc;
}
