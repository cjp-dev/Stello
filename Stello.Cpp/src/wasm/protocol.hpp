#pragma once

#include "core/board.hpp"
#include "search/limits.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace stello::wasm {

inline constexpr uint32_t DEFAULT_HASH_BITS = 19;

struct MoveRequest {
    Board board;
    Player player{Player::Black};
    SearchLimits limits{SearchLimits::solve()};
    uint32_t hash_bits{DEFAULT_HASH_BITS};
};

struct ReadRequestResult {
    bool ok{false};
    MoveRequest request{};
    std::string error;
};

ReadRequestResult read_request(std::string_view json);
std::string write_result(const SearchResult& result);
std::string write_progress(const SearchInfo& info);

} // namespace stello::wasm
