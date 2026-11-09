#pragma once

#include "cli/options.hpp"
#include "core/board.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace stello::cli {

struct BenchPosition {
    std::string id;
    Board board;
    Player player{Player::Black};
    std::optional<int32_t> score;
    std::vector<std::string> best_moves;
};

bool load_positions(const std::filesystem::path& path, std::vector<BenchPosition>& out, std::string& error_out);
std::vector<BenchPosition> select_by_id_range(std::vector<BenchPosition> all, int32_t from, int32_t to);

int cmd_solve_bench(std::span<const std::string_view> args, std::string& error_out);
int cmd_mid_bench(std::span<const std::string_view> args, std::string& error_out);

} // namespace stello::cli
