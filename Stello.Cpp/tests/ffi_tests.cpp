#include "tests/test_harness.hpp"

#include "core/board.hpp"
#include "eval/weights.hpp"
#include "ffi/stello_ffi.hpp"
#include "search/limits.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace stello::tests {
namespace {

void count_reports(const StelloSearchInfo* /*info*/, void* user) {
    auto* count = static_cast<uint32_t*>(user);
    ++(*count);
}

void record_iteration(int32_t depth, int32_t /*score*/, void* user) {
    auto* iterations = static_cast<std::vector<int32_t>*>(user);
    iterations->push_back(depth);
}

std::pair<StelloEngine*, int32_t> create_ffi_engine(int32_t hash_bits) {
    int32_t status = -1;
    StelloEngine* engine = stello_engine_create(hash_bits, 1, -1, -1, nullptr, 0, &status);
    return {engine, status};
}

} // namespace

STELLO_TEST(ffi_version_and_layout) {
    STELLO_CHECK_EQ(stello_version(), 40000u);
    STELLO_CHECK_EQ(stello_abi_version(), 3u);
    STELLO_CHECK_EQ(sizeof(StelloSearchInfo), size_t{48});
    STELLO_CHECK_EQ(offsetof(StelloSearchInfo, depth), size_t{24});
    STELLO_CHECK_EQ(sizeof(StelloSearchResult), size_t{40});
    STELLO_CHECK_EQ(offsetof(StelloSearchResult, mv), size_t{24});
}

STELLO_TEST(ffi_create_checks_its_arguments) {
    {
        auto [engine, status] = create_ffi_engine(9);
        STELLO_CHECK(engine == nullptr);
        STELLO_CHECK_EQ(status, STELLO_INVALID_ARGUMENT);
    }
    {
        int32_t status = -1;
        STELLO_CHECK(stello_engine_create(27, 1, -1, -1, nullptr, 0, &status) == nullptr);
        STELLO_CHECK_EQ(status, STELLO_INVALID_ARGUMENT);
        STELLO_CHECK(stello_engine_create(12, 1, -2, -1, nullptr, 0, &status) == nullptr);
        STELLO_CHECK_EQ(status, STELLO_INVALID_ARGUMENT);
        STELLO_CHECK(stello_engine_create(12, 1, 100001, -1, nullptr, 0, &status) == nullptr);
        STELLO_CHECK_EQ(status, STELLO_INVALID_ARGUMENT);
        STELLO_CHECK(stello_engine_create(12, 1, -1, -2, nullptr, 0, &status) == nullptr);
        STELLO_CHECK_EQ(status, STELLO_INVALID_ARGUMENT);
        STELLO_CHECK(stello_engine_create(12, 1, -1, 101, nullptr, 0, &status) == nullptr);
        STELLO_CHECK_EQ(status, STELLO_INVALID_ARGUMENT);
    }

    {
        const std::array<uint8_t, 3> bytes = {1, 2, 3};
        int32_t status = -1;
        StelloEngine* engine = stello_engine_create(12, 1, -1, -1, bytes.data(), bytes.size(), &status);
        STELLO_CHECK(engine == nullptr);
        STELLO_CHECK_EQ(status, STELLO_INVALID_EVALUATION);
    }

    {
        const auto file = eval::EvalWeights::built_in_file();
        int32_t status = -1;
        StelloEngine* engine = stello_engine_create(12, 0, 150, 50, file.data(), file.size(), &status);
        STELLO_CHECK(engine != nullptr);
        STELLO_CHECK_EQ(status, STELLO_OK);
        stello_engine_destroy(engine);
    }
}

STELLO_TEST(ffi_search_checks_its_arguments_and_finds_a_move) {
    auto [engine, status] = create_ffi_engine(12);
    STELLO_CHECK(engine != nullptr);
    STELLO_CHECK_EQ(status, STELLO_OK);

    const Board initial = Board::initial();
    StelloSearchResult result{};
    const auto search = [&](int32_t player, int32_t mode, int32_t depth, std::optional<uint64_t> only) {
        return stello_engine_search(
            engine,
            initial.black(),
            initial.white(),
            player,
            mode,
            depth,
            0,
            only.value_or(0),
            only.has_value() ? 1 : 0,
            nullptr,
            nullptr,
            nullptr,
            &result);
    };

    STELLO_CHECK_EQ(search(2, 0, 4, std::nullopt), STELLO_INVALID_ARGUMENT);
    STELLO_CHECK_EQ(search(0, 4, 4, std::nullopt), STELLO_INVALID_ARGUMENT);
    STELLO_CHECK_EQ(search(0, 0, 61, std::nullopt), STELLO_INVALID_ARGUMENT);
    STELLO_CHECK_EQ(search(0, 0, 4, uint64_t{1}), STELLO_NO_LEGAL_ONLY_MOVE);
    STELLO_CHECK_EQ(search(0, 0, 4, std::nullopt), STELLO_OK);
    STELLO_CHECK_EQ(result.depth, 4);
    STELLO_CHECK_EQ(result.kind, static_cast<int32_t>(ScoreKind::Heuristic));
    STELLO_CHECK(result.mv >= 0 && result.mv < 64);
    STELLO_CHECK((initial.legal_moves(Player::Black) & (uint64_t{1} << result.mv)) != 0);
    STELLO_CHECK_EQ(stello_engine_clear_hash(engine), STELLO_OK);
    STELLO_CHECK_EQ(stello_engine_clear_hash(nullptr), STELLO_INVALID_ARGUMENT);
    STELLO_CHECK_EQ(
        stello_engine_search(engine, 1, 1, 0, 0, 1, 0, 0, 0, nullptr, nullptr, nullptr, &result),
        STELLO_INVALID_ARGUMENT);
    STELLO_CHECK_EQ(
        stello_engine_search(nullptr, 0, 0, 0, 0, 1, 0, 0, 0, nullptr, nullptr, nullptr, nullptr),
        STELLO_INVALID_ARGUMENT);

    stello_engine_destroy(engine);
    stello_engine_destroy(nullptr);
}

STELLO_TEST(ffi_threads_are_checked_and_searches_with_them_finish) {
    auto [engine, status] = create_ffi_engine(12);
    STELLO_CHECK(engine != nullptr);
    STELLO_CHECK_EQ(status, STELLO_OK);

    STELLO_CHECK_EQ(stello_engine_set_threads(nullptr, 2), STELLO_INVALID_ARGUMENT);
    STELLO_CHECK_EQ(stello_engine_set_threads(engine, 0), STELLO_INVALID_ARGUMENT);
    STELLO_CHECK_EQ(stello_engine_set_threads(engine, 257), STELLO_INVALID_ARGUMENT);
    STELLO_CHECK_EQ(stello_engine_set_threads(engine, 4), STELLO_OK);
    STELLO_CHECK_EQ(stello_engine_set_ybwc(nullptr, 0), STELLO_INVALID_ARGUMENT);
    STELLO_CHECK_EQ(stello_engine_set_ybwc(engine, 0), STELLO_OK);

    const Board initial = Board::initial();
    StelloSearchResult result{};
    for (int32_t depth : {6, 8}) {
        const int32_t code = stello_engine_search(
            engine,
            initial.black(),
            initial.white(),
            0,
            0,
            depth,
            0,
            0,
            0,
            nullptr,
            nullptr,
            nullptr,
            &result);
        STELLO_CHECK_EQ(code, STELLO_OK);
        STELLO_CHECK_EQ(result.depth, depth);
        STELLO_CHECK(result.mv >= 0 && result.mv < 64);
        STELLO_CHECK((initial.legal_moves(Player::Black) & (uint64_t{1} << result.mv)) != 0);
    }

    STELLO_CHECK_EQ(stello_engine_set_threads(engine, 1), STELLO_OK);
    stello_engine_destroy(engine);
}

STELLO_TEST(ffi_callbacks_and_signals) {
    auto [engine, status] = create_ffi_engine(12);
    STELLO_CHECK(engine != nullptr);
    STELLO_CHECK_EQ(status, STELLO_OK);

    std::vector<int32_t> iterations;
    uint32_t reports = 0;
    StelloSignals* signals = stello_signals_create();
    STELLO_CHECK(signals != nullptr);

    const Board initial = Board::initial();
    StelloSearchResult result{};

    STELLO_CHECK_EQ(stello_engine_set_iteration_done(nullptr, &record_iteration, &iterations), STELLO_INVALID_ARGUMENT);
    STELLO_CHECK_EQ(stello_engine_set_iteration_done(engine, &record_iteration, &iterations), STELLO_OK);
    STELLO_CHECK_EQ(
        stello_engine_search(
            engine,
            initial.black(),
            initial.white(),
            0,
            0,
            5,
            0,
            0,
            0,
            signals,
            &count_reports,
            &reports,
            &result),
        STELLO_OK);
    STELLO_CHECK_EQ(stello_engine_set_iteration_done(engine, nullptr, nullptr), STELLO_OK);

    stello_signals_cancel(signals);
    STELLO_CHECK_EQ(
        stello_engine_search(
            engine,
            initial.black(),
            initial.white(),
            0,
            0,
            5,
            0,
            0,
            0,
            signals,
            nullptr,
            nullptr,
            &result),
        STELLO_CANCELLED);

    stello_signals_reset(signals);
    stello_signals_move_now(signals);
    STELLO_CHECK_EQ(
        stello_engine_search(
            engine,
            initial.black(),
            initial.white(),
            0,
            3,
            0,
            0,
            0,
            0,
            signals,
            nullptr,
            nullptr,
            &result),
        STELLO_OK);

    stello_signals_destroy(signals);
    stello_signals_destroy(nullptr);
    stello_engine_destroy(engine);

    const std::vector<int32_t> expected = {1, 2, 3, 4, 5};
    STELLO_CHECK(iterations == expected);
    STELLO_CHECK(reports >= 20u);
}

} // namespace stello::tests
