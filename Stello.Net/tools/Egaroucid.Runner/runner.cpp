// SPDX-License-Identifier: GPL-3.0-only
//
// Persistent subprocess runner for native C++ Egaroucid (Egaroucid-main/src/engine,
// (c) Takuto Yamana, GPL-3.0-or-later) used by Stello.Benchmark (engine=egaroucid-cpp).

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include "engine/engine_all.hpp"

namespace {

bool load_hash_tables(const std::string &res_dir, int hash_bits) {
    const int file_level = (hash_bits >= 25 && hash_bits <= 27) ? hash_bits : 25;
    const std::string path = res_dir + "hash/hash" + std::to_string(file_level) + ".eghs";
    FILE *fp = nullptr;
    if (!file_open(&fp, path.c_str(), "rb")) {
        std::cerr << "[ERROR] cannot open " << path << std::endl;
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        if (fread(hash_rand_player[i], sizeof(uint32_t), 65536, fp) < 65536) {
            std::cerr << "[ERROR] broken hash file " << path << std::endl;
            fclose(fp);
            return false;
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (fread(hash_rand_opponent[i], sizeof(uint32_t), 65536, fp) < 65536) {
            std::cerr << "[ERROR] broken hash file " << path << std::endl;
            fclose(fp);
            return false;
        }
    }
    fclose(fp);

    if (hash_bits < 25) {
        const uint32_t mask = static_cast<uint32_t>(hash_sizes[hash_bits] - 1);
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 65536; ++j) {
                hash_rand_player[i][j] &= mask;
                hash_rand_opponent[i][j] &= mask;
            }
        }
    }
    return true;
}

std::string trim_dir(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '"')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n' || s.back() == '"')) {
        s.pop_back();
    }
    std::replace(s.begin(), s.end(), '\\', '/');
    if (!s.empty() && s.back() != '/') {
        s.push_back('/');
    }
    return s;
}

} // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    bool initialized = false;
    bool use_mt = false;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "init") {
            int threads = 1;
            int hash_bits = 21;
            std::string res_dir_raw;
            if (!(iss >> threads >> hash_bits)) {
                std::cerr << "[ERROR] usage: init <threads> <hashBits> <resourceDir>" << std::endl;
                return 1;
            }
            std::getline(iss, res_dir_raw);
            std::string res_dir = trim_dir(res_dir_raw);
            if (threads < 1 || hash_bits < 10 || hash_bits > 27 || res_dir.empty()) {
                std::cerr << "[ERROR] invalid init arguments" << std::endl;
                return 1;
            }

            if (!initialized) {
                const int workers = std::max(0, threads - 1);
                thread_pool.resize(workers);
                use_mt = (threads > 1);

                bit_init();
                mobility_init();
                flip_init();
                last_flip_init();
                endsearch_init();
#if USE_MPC_PRE_CALCULATION
                mpc_init();
#endif
                move_ordering_init();
                if (!transposition_table.resize(hash_bits)) {
                    std::cerr << "[ERROR] transposition_table.resize failed for level " << hash_bits << std::endl;
                    return 1;
                }
                if (!load_hash_tables(res_dir, hash_bits)) {
                    return 1;
                }
#if USE_CHANGEABLE_HASH_LEVEL
                global_hash_level = hash_bits;
#endif
#if USE_CRC32C_HASH
                global_hash_bit_mask = static_cast<uint32_t>(hash_sizes[hash_bits] - 1);
#endif
                stability_init();
                if (!evaluate_init(res_dir + "eval.egev2", res_dir + "eval_move_ordering_end.egev", false)) {
                    std::cerr << "[ERROR] cannot load evaluation from " << res_dir << std::endl;
                    return 1;
                }
                initialized = true;
            }
            std::cout << "ready\n" << std::flush;
        } else if (cmd == "clear") {
            if (!initialized) {
                std::cerr << "[ERROR] not initialized" << std::endl;
                return 1;
            }
            transposition_table.init();
            std::cout << "ok\n" << std::flush;
        } else if (cmd == "go") {
            if (!initialized) {
                std::cerr << "[ERROR] not initialized" << std::endl;
                return 1;
            }
            uint64_t own = 0;
            uint64_t opp = 0;
            std::string mode;
            if (!(iss >> std::hex >> own >> opp >> mode)) {
                std::cerr << "[ERROR] invalid go command: " << line << std::endl;
                return 1;
            }
            iss >> std::dec;

            Board b(own, opp);
            Search_result res;
            bool searching = true;
            const auto started = std::chrono::steady_clock::now();

            if (mode == "movetime") {
                uint64_t ms = 100;
                iss >> ms;
                res = ai_common(b, -SCORE_MAX, SCORE_MAX, MAX_LEVEL, false, 0, use_mt, false, b.get_legal(), false, std::max<uint64_t>(1, ms), THREAD_ID_NONE, &searching);
            } else if (mode == "gametime") {
                uint64_t ms = 1000;
                iss >> ms;
                res = ai_time_limit(b, false, 0, use_mt, false, std::max<uint64_t>(1, ms), THREAD_ID_NONE, &searching);
            } else if (mode == "level") {
                int level = 21;
                iss >> level;
                res = ai(b, level, false, 0, use_mt, false);
            } else if (mode == "depth") {
                int depth = 10;
                int mpc_idx = MPC_100_LEVEL;
                iss >> depth >> mpc_idx;
                res = tree_search_legal(b, -SCORE_MAX, SCORE_MAX, depth, static_cast<uint_fast8_t>(mpc_idx), false, b.get_legal(), use_mt, TIME_LIMIT_INF, THREAD_ID_NONE, &searching);
            } else if (mode == "solve") {
                res = ai(b, MAX_LEVEL, false, 0, use_mt, false);
            } else {
                std::cerr << "[ERROR] unknown go mode: " << mode << std::endl;
                return 1;
            }

            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();

            uint64_t legal = b.get_legal();
            if (legal != 0ULL && (!is_valid_policy(res.policy) || (legal & (1ULL << res.policy)) == 0ULL)) {
                res.policy = static_cast<int_fast8_t>(first_bit(&legal));
            }

            const uint64_t total_nodes = res.nodes + res.clog_nodes;
            std::cout
                << "bestmove " << static_cast<int>(res.policy)
                << " score " << res.value
                << " depth " << res.depth
                << " prob " << res.probability
                << " end " << (res.is_end_search ? 1 : 0)
                << " nodes " << total_nodes
                << " ms " << elapsed_ms
                << '\n' << std::flush;
        } else if (cmd == "quit") {
            break;
        } else {
            std::cerr << "[ERROR] unknown command: " << cmd << std::endl;
            return 1;
        }
    }

    if (initialized) {
        thread_pool.exit_thread();
    }
    return 0;
}
