#include "cli/bench.hpp"

#include "core/cpu.hpp"
#include "eval/weights.hpp"
#include "search/engine.hpp"
#include "search/limits.hpp"
#include "search/options.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <thread>

namespace stello::cli {

namespace {

struct BenchResult {
    std::string id;
    uint32_t empties{0};
    std::string mv;
    int32_t score{0};
    std::string_view kind;
    int32_t depth{0};
    uint64_t nodes{0};
    double ms{0.0};
};

std::string trim_copy(std::string_view sv) {
    size_t start = 0;
    while (start < sv.size() && (sv[start] == ' ' || sv[start] == '\t' || sv[start] == '\r' || sv[start] == '\n')) {
        ++start;
    }
    size_t end = sv.size();
    while (end > start && (sv[end - 1] == ' ' || sv[end - 1] == '\t' || sv[end - 1] == '\r' || sv[end - 1] == '\n')) {
        --end;
    }
    return std::string(sv.substr(start, end - start));
}

template <typename Int>
bool parse_number(
    const Options& options,
    std::string_view name,
    Int default_val,
    Int min_val,
    Int max_val,
    Int& out,
    std::string& error_out
) {
    const auto txt = options.text(name);
    if (!txt.has_value()) {
        out = default_val;
        return true;
    }
    Int val{};
    const auto* begin = txt->data();
    const auto* end = begin + txt->size();
    const auto [ptr, ec] = std::from_chars(begin, end, val);
    if (ec != std::errc{} || ptr != end || val < min_val || val > max_val) {
        std::ostringstream oss;
        oss << "--" << name << " must be a whole number from " << min_val << " to " << max_val << ".";
        error_out = oss.str();
        return false;
    }
    out = val;
    return true;
}

bool engine_options(const Options& options, SearchOptions& search_out, std::string& text_out, std::string& error_out) {
    search_out = SearchOptions{};
    text_out.clear();

    if (const auto cpu_opt = options.text("cpu")) {
        if (*cpu_opt == "portable") {
            stello::cpu::set_portable_only(true);
        } else if (*cpu_opt != "auto") {
            error_out = "--cpu must be auto or portable.";
            return false;
        }
    }

    if (const auto eval_path = options.text("eval")) {
        if (*eval_path != "default") {
            eval::EvalError eval_err;
            auto loaded = eval::EvalWeights::load(std::filesystem::path(*eval_path), &eval_err);
            if (!loaded) {
                error_out = std::string(*eval_path) + ": " + eval_err.message();
                return false;
            }
            search_out.pattern = std::make_shared<const eval::EvalWeights>(std::move(*loaded));
            const std::string name = std::filesystem::path(*eval_path).filename().string();
            text_out += ", evaluation " + name;
        }
    }

    if (const auto sel = options.text("selective")) {
        if (*sel == "off") {
            search_out.selective = false;
            text_out += ", selective off";
        } else if (*sel != "on") {
            error_out = "--selective must be on or off.";
            return false;
        }
    }

    if (const auto ybwc_opt = options.text("ybwc")) {
        if (*ybwc_opt == "off") {
            search_out.ybwc = false;
            text_out += ", YBWC off";
        } else if (*ybwc_opt != "on") {
            error_out = "--ybwc must be on or off.";
            return false;
        }
    }

    if (options.text("probcut-pct").has_value()) {
        int32_t percent = 100;
        if (!parse_number(options, "probcut-pct", 100, 1, 1000, percent, error_out)) {
            return false;
        }
        search_out.probcut_percent = percent;
        text_out += ", probcut " + std::to_string(percent) + " %";
    }

    return true;
}

bool with_threads(const Options& options, SearchLimits base, SearchLimits& limits_out, std::string& text_out, std::string& error_out) {
    uint32_t threads = 1;
    if (!parse_number(options, "threads", uint32_t{1}, uint32_t{1}, SearchLimits::MAX_THREADS, threads, error_out)) {
        return false;
    }
    const auto with_t = base.with_threads(threads);
    limits_out = *with_t;
    text_out = (threads > 1) ? (", " + std::to_string(threads) + " threads") : std::string{};
    return true;
}

bool resolve_positions_path(
    const Options& options,
    std::string_view default_name,
    std::filesystem::path& path_out,
    std::string& error_out
) {
    if (const auto custom = options.text("positions")) {
        path_out = std::filesystem::path(*custom);
        return true;
    }
    const auto root = repository_folder();
    if (!root) {
        error_out = "No --positions given and no repository found.";
        return false;
    }
    path_out = *root / "Stello.Net" / "tools" / "Stello.Benchmark" / "Data" / std::string(default_name);
    return true;
}

std::string signed_score(int32_t score) {
    if (score > 0) {
        return "+" + std::to_string(score);
    }
    return std::to_string(score);
}

std::string format_seconds(double ms) {
    char buf[32];
    if (ms >= 10000.0) {
        std::snprintf(buf, sizeof(buf), "%.1f s", ms / 1000.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%.3f s", ms / 1000.0);
    }
    return std::string(buf);
}

std::string format_speed(uint64_t nodes, double ms) {
    if (ms > 0.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2fM", static_cast<double>(nodes) / ms / 1000.0);
        return std::string(buf);
    }
    return "-";
}

std::string json_quote(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');
    for (unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            default:
                if (c < 0x20U) {
                    char esc[8];
                    std::snprintf(esc, sizeof(esc), "\\u%04x", static_cast<unsigned>(c));
                    out += esc;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
    return out;
}

std::string format_double_json(double v) {
    char buf[64];
    const auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    if (ec == std::errc{}) {
        return std::string(buf, ptr);
    }
    std::snprintf(buf, sizeof(buf), "%.6f", v);
    return std::string(buf);
}

std::string utc_now_string() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm{};
#if defined(_WIN32)
    gmtime_s(&utc_tm, &t);
#else
    gmtime_r(&t, &utc_tm);
#endif
    char buf[32];
    std::snprintf(
        buf,
        sizeof(buf),
        "%04d-%02d-%02d %02d:%02d",
        utc_tm.tm_year + 1900,
        utc_tm.tm_mon + 1,
        utc_tm.tm_mday,
        utc_tm.tm_hour,
        utc_tm.tm_min);
    return std::string(buf);
}

std::string bench_run_json(
    std::string_view command,
    std::string_view limits,
    uint32_t hash_bits,
    uint32_t passes,
    const std::vector<BenchResult>& results
) {
    unsigned int processors = std::thread::hardware_concurrency();
    if (processors == 0) {
        processors = 1;
    }

    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"Command\": " << json_quote(command) << ",\n";
    oss << "  \"Date\": " << json_quote(utc_now_string()) << ",\n";
    oss << "  \"ProcessorCount\": " << processors << ",\n";
    oss << "  \"Limits\": " << json_quote(limits) << ",\n";
    oss << "  \"HashBits\": " << hash_bits << ",\n";
    oss << "  \"Passes\": " << passes << ",\n";
    oss << "  \"Results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const BenchResult& r = results[i];
        oss << "    {\n";
        oss << "      \"Id\": " << json_quote(r.id) << ",\n";
        oss << "      \"Empties\": " << r.empties << ",\n";
        oss << "      \"Move\": " << json_quote(r.mv) << ",\n";
        oss << "      \"Score\": " << r.score << ",\n";
        oss << "      \"Kind\": " << json_quote(r.kind) << ",\n";
        oss << "      \"Depth\": " << r.depth << ",\n";
        oss << "      \"Nodes\": " << r.nodes << ",\n";
        oss << "      \"Ms\": " << format_double_json(r.ms) << "\n";
        oss << ((i + 1 < results.size()) ? "    },\n" : "    }\n");
    }
    oss << "  ],\n";
    oss << "  \"Engine\": \"stello-cpp\"\n";
    oss << "}";
    return oss.str();
}

bool save_json(
    const Options& options,
    std::string_view command,
    std::string_view limits_text,
    uint32_t hash_bits,
    uint32_t passes,
    const std::vector<BenchResult>& results,
    std::string& error_out
) {
    const auto out_opt = options.text("out");
    if (!out_opt.has_value()) {
        return true;
    }
    const std::filesystem::path out_path(*out_opt);
    std::ofstream file(out_path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error_out = std::string(*out_opt) + ": cannot open output file.";
        return false;
    }
    const std::string json = bench_run_json(command, limits_text, hash_bits, passes, results);
    file.write(json.data(), static_cast<std::streamsize>(json.size()));
    if (!file) {
        error_out = std::string(*out_opt) + ": write failed.";
        return false;
    }
    std::printf("Saved to %.*s.\n", static_cast<int>(out_opt->size()), out_opt->data());
    return true;
}

bool run_benchmark(
    std::string_view command,
    const std::vector<BenchPosition>& positions,
    search::SearchEngine& engine,
    const SearchLimits& limits,
    std::string_view limits_text,
    uint32_t hash_bits,
    uint32_t passes,
    std::vector<BenchResult>& results_out,
    std::string& error_out
) {
    if (positions.empty()) {
        error_out = "No positions to search.";
        return false;
    }

    std::printf(
        "%.*s: %zu positions, stello-cpp, %.*s, hash bits %u, %u pass(es).\n",
        static_cast<int>(command.size()),
        command.data(),
        positions.size(),
        static_cast<int>(limits_text.size()),
        limits_text.data(),
        hash_bits,
        passes);

    std::vector<std::optional<BenchResult>> best(positions.size());
    for (uint32_t pass = 1; pass <= passes; ++pass) {
        const auto pass_t0 = std::chrono::steady_clock::now();
        uint64_t nodes = 0;

        for (size_t i = 0; i < positions.size(); ++i) {
            const BenchPosition& position = positions[i];
            engine.clear_hash();
            const SearchResult result = engine.search_simple(position.board, position.player, limits);

            BenchResult current;
            current.id = position.id;
            current.empties = position.board.empty_count();
            current.mv = result.mv.to_string();
            current.score = result.score;
            current.kind = score_kind_name(result.kind);
            current.depth = result.depth;
            current.nodes = result.nodes;
            current.ms = std::chrono::duration<double, std::milli>(result.elapsed).count();
            nodes += result.nodes;

            if (best[i].has_value() && limits.threads() == 1
                && (best[i]->nodes != current.nodes || best[i]->mv != current.mv || best[i]->score != current.score))
            {
                std::printf(
                    "  Warning: %s gave another result in pass %u (%llu nodes, %s).\n",
                    position.id.c_str(),
                    pass,
                    static_cast<unsigned long long>(current.nodes),
                    current.mv.c_str());
            }

            if (command == "solve-bench" && positions.size() <= 100) {
                std::printf(
                    "  pass %u #%s: %s %s in %s (%llu nodes)\n",
                    pass,
                    current.id.c_str(),
                    signed_score(current.score).c_str(),
                    current.mv.c_str(),
                    format_seconds(current.ms).c_str(),
                    static_cast<unsigned long long>(current.nodes));
            }

            if (!best[i].has_value() || best[i]->ms > current.ms) {
                best[i] = std::move(current);
            }
        }

        const double pass_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - pass_t0).count();
        std::printf("  pass %u: %llu nodes in %.2f s\n", pass, static_cast<unsigned long long>(nodes), pass_s);
    }

    results_out.clear();
    results_out.reserve(positions.size());
    for (auto& slot : best) {
        results_out.push_back(std::move(*slot));
    }
    return true;
}

} // namespace

bool load_positions(const std::filesystem::path& path, std::vector<BenchPosition>& out, std::string& error_out) {
    std::ifstream file(path);
    if (!file) {
        error_out = path.string() + ": cannot open file.";
        return false;
    }

    out.clear();
    std::string line;
    while (std::getline(file, line)) {
        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::vector<std::string> parts;
        {
            std::istringstream semi(trimmed);
            std::string item;
            while (std::getline(semi, item, ';')) {
                const std::string part = trim_copy(item);
                if (!part.empty()) {
                    parts.push_back(part);
                }
            }
        }

        std::vector<std::string> head;
        if (!parts.empty()) {
            std::istringstream ws(parts[0]);
            std::string tok;
            while (ws >> tok) {
                head.push_back(tok);
            }
        }

        if (head.size() != 3 || head[1].size() != 64) {
            error_out = "Not a position: " + line;
            return false;
        }

        BoardParseError parse_err;
        const auto board = Board::parse(head[1], &parse_err);
        if (!board) {
            error_out = parse_err.message() + ": " + line;
            return false;
        }

        Player player = Player::Black;
        if (head[2] == "X" || head[2] == "x") {
            player = Player::Black;
        } else if (head[2] == "O" || head[2] == "o") {
            player = Player::White;
        } else {
            error_out = "The side to move must be X or O: " + line;
            return false;
        }

        std::vector<std::pair<std::string, int32_t>> scored;
        for (size_t p = 1; p < parts.size(); ++p) {
            const std::string& part = parts[p];
            const size_t colon = part.find(':');
            if (colon == std::string::npos) {
                error_out = "Not a move with a score: " + part;
                return false;
            }
            const std::string mv_text = part.substr(0, colon);
            std::string_view sc_text(part.data() + colon + 1, part.size() - colon - 1);
            if (!sc_text.empty() && sc_text[0] == '+') {
                sc_text.remove_prefix(1);
            }
            int32_t sc = 0;
            const auto [ptr, ec] = std::from_chars(sc_text.data(), sc_text.data() + sc_text.size(), sc);
            const auto sq = Square::parse(mv_text);
            if (ec != std::errc{} || ptr != sc_text.data() + sc_text.size() || !sq.has_value()) {
                error_out = "Not a move with a score: " + part;
                return false;
            }
            scored.emplace_back(sq->to_string(), sc);
        }

        std::optional<int32_t> best_score;
        for (const auto& [m, sc] : scored) {
            (void)m;
            if (!best_score.has_value() || sc > *best_score) {
                best_score = sc;
            }
        }
        std::vector<std::string> best_moves;
        if (best_score.has_value()) {
            for (const auto& [m, sc] : scored) {
                if (sc == *best_score) {
                    best_moves.push_back(m);
                }
            }
        }

        out.push_back(BenchPosition{head[0], *board, player, best_score, std::move(best_moves)});
    }

    return true;
}

std::vector<BenchPosition> select_by_id_range(std::vector<BenchPosition> all, int32_t from, int32_t to) {
    std::vector<BenchPosition> out;
    out.reserve(all.size());
    for (auto& p : all) {
        int32_t id = 0;
        const auto [ptr, ec] = std::from_chars(p.id.data(), p.id.data() + p.id.size(), id);
        if (ec != std::errc{} || ptr != p.id.data() + p.id.size() || (id >= from && id <= to)) {
            out.push_back(std::move(p));
        }
    }
    return out;
}

int cmd_solve_bench(std::span<const std::string_view> args, std::string& error_out) {
    static constexpr std::array<std::string_view, 10> kAllowed = {
        "positions", "from", "to", "passes", "hash-bits", "eval", "out", "cpu", "threads", "ybwc"
    };
    const auto opts = Options::parse(args, kAllowed, error_out);
    if (!opts) {
        return 1;
    }

    std::filesystem::path pos_path;
    if (!resolve_positions_path(*opts, "ffo-40-59.txt", pos_path, error_out)) {
        return 1;
    }

    std::vector<BenchPosition> all;
    if (!load_positions(pos_path, all, error_out)) {
        return 1;
    }

    int32_t from = 40;
    int32_t to = 44;
    uint32_t passes = 1;
    uint32_t hash_bits = 19;
    if (!parse_number(*opts, "from", 40, 0, (std::numeric_limits<int32_t>::max)(), from, error_out)
        || !parse_number(*opts, "to", 44, 0, (std::numeric_limits<int32_t>::max)(), to, error_out)
        || !parse_number(*opts, "passes", uint32_t{1}, uint32_t{1}, uint32_t{100}, passes, error_out)
        || !parse_number(*opts, "hash-bits", uint32_t{19}, uint32_t{10}, uint32_t{26}, hash_bits, error_out))
    {
        return 1;
    }

    std::vector<BenchPosition> positions = select_by_id_range(std::move(all), from, to);

    SearchOptions search_options;
    std::string options_text;
    if (!engine_options(*opts, search_options, options_text, error_out)) {
        return 1;
    }

    SearchLimits limits = SearchLimits::solve();
    std::string threads_text;
    if (!with_threads(*opts, SearchLimits::solve(), limits, threads_text, error_out)) {
        return 1;
    }

    const std::string limits_text = "solve" + options_text + threads_text;
    search::SearchEngine engine(hash_bits, std::move(search_options));
    std::vector<BenchResult> results;
    if (!run_benchmark("solve-bench", positions, engine, limits, limits_text, hash_bits, passes, results, error_out)) {
        return 1;
    }

    size_t wrong = 0;
    std::printf("\n| # | Empty | Score | Move | Nodes | Time | Nodes/s |\n");
    std::printf("|---|---:|---:|---|---:|---:|---:|\n");
    for (size_t i = 0; i < positions.size(); ++i) {
        const BenchPosition& position = positions[i];
        const BenchResult& result = results[i];
        const bool score_ok = !position.score.has_value() || (result.kind == "Exact" && result.score == *position.score);
        const bool move_ok = position.best_moves.empty()
                          || std::find(position.best_moves.begin(), position.best_moves.end(), result.mv) != position.best_moves.end();
        if (!score_ok || !move_ok) {
            ++wrong;
        }

        const std::string score_note = score_ok ? std::string{} : (" (should be " + signed_score(*position.score) + ")");
        const char* move_note = move_ok ? "" : " (wrong)";
        std::printf(
            "| %s | %u | %s%s | %s%s | %llu | %s | %s |\n",
            result.id.c_str(),
            result.empties,
            signed_score(result.score).c_str(),
            score_note.c_str(),
            result.mv.c_str(),
            move_note,
            static_cast<unsigned long long>(result.nodes),
            format_seconds(result.ms).c_str(),
            format_speed(result.nodes, result.ms).c_str());
    }

    uint64_t total_nodes = 0;
    double total_ms = 0.0;
    for (const auto& r : results) {
        total_nodes += r.nodes;
        total_ms += r.ms;
    }
    std::printf(
        "| **Total** | | | | **%llu** | **%s** | **%s** |\n",
        static_cast<unsigned long long>(total_nodes),
        format_seconds(total_ms).c_str(),
        format_speed(total_nodes, total_ms).c_str());
    if (wrong == 0) {
        std::printf("All scores and moves are correct.\n");
    } else {
        std::printf("%zu positions have a wrong score or move.\n", wrong);
    }

    if (!save_json(*opts, "solve-bench", limits_text, hash_bits, passes, results, error_out)) {
        return 1;
    }
    if (wrong > 0) {
        error_out = std::to_string(wrong) + " wrong.";
        return 1;
    }
    return 0;
}

int cmd_mid_bench(std::span<const std::string_view> args, std::string& error_out) {
    static constexpr std::array<std::string_view, 11> kAllowed = {
        "positions", "depth", "passes", "hash-bits", "eval", "selective", "probcut-pct", "out", "cpu", "threads", "ybwc"
    };
    const auto opts = Options::parse(args, kAllowed, error_out);
    if (!opts) {
        return 1;
    }

    std::filesystem::path pos_path;
    if (!resolve_positions_path(*opts, "midgame.txt", pos_path, error_out)) {
        return 1;
    }

    std::vector<BenchPosition> positions;
    if (!load_positions(pos_path, positions, error_out)) {
        return 1;
    }

    int32_t depth = 10;
    uint32_t passes = 3;
    uint32_t hash_bits = 19;
    if (!parse_number(*opts, "depth", 10, 1, 60, depth, error_out)
        || !parse_number(*opts, "passes", uint32_t{3}, uint32_t{1}, uint32_t{100}, passes, error_out)
        || !parse_number(*opts, "hash-bits", uint32_t{19}, uint32_t{10}, uint32_t{26}, hash_bits, error_out))
    {
        return 1;
    }

    SearchOptions search_options;
    std::string options_text;
    if (!engine_options(*opts, search_options, options_text, error_out)) {
        return 1;
    }

    SearchLimits limits = SearchLimits::fixed_depth(depth);
    std::string threads_text;
    if (!with_threads(*opts, SearchLimits::fixed_depth(depth), limits, threads_text, error_out)) {
        return 1;
    }

    const std::string limits_text = "depth " + std::to_string(depth) + options_text + threads_text;
    search::SearchEngine engine(hash_bits, std::move(search_options));
    std::vector<BenchResult> results;
    if (!run_benchmark("mid-bench", positions, engine, limits, limits_text, hash_bits, passes, results, error_out)) {
        return 1;
    }

    std::printf("\n| Empty squares | Positions | Nodes | Time | Nodes/s |\n");
    std::printf("|---:|---:|---:|---:|---:|\n");

    std::vector<uint32_t> empties;
    empties.reserve(results.size());
    for (const auto& r : results) {
        empties.push_back(r.empties);
    }
    std::sort(empties.begin(), empties.end(), std::greater<uint32_t>());
    empties.erase(std::unique(empties.begin(), empties.end()), empties.end());

    for (const uint32_t e : empties) {
        size_t group_count = 0;
        uint64_t group_nodes = 0;
        double group_ms = 0.0;
        for (const auto& r : results) {
            if (r.empties == e) {
                ++group_count;
                group_nodes += r.nodes;
                group_ms += r.ms;
            }
        }
        std::printf(
            "| %u | %zu | %llu | %s | %s |\n",
            e,
            group_count,
            static_cast<unsigned long long>(group_nodes),
            format_seconds(group_ms).c_str(),
            format_speed(group_nodes, group_ms).c_str());
    }

    uint64_t total_nodes = 0;
    double total_ms = 0.0;
    for (const auto& r : results) {
        total_nodes += r.nodes;
        total_ms += r.ms;
    }
    std::printf(
        "| **Total** | %zu | **%llu** | **%s** | **%s** |\n",
        results.size(),
        static_cast<unsigned long long>(total_nodes),
        format_seconds(total_ms).c_str(),
        format_speed(total_nodes, total_ms).c_str());

    if (!save_json(*opts, "mid-bench", limits_text, hash_bits, passes, results, error_out)) {
        return 1;
    }
    return 0;
}

} // namespace stello::cli
