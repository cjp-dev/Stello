#include "tests/test_harness.hpp"

#include "wasm/protocol.hpp"

#include <chrono>
#include <cstdint>
#include <string_view>

namespace stello::tests {

STELLO_TEST(wasm_protocol_reads_requests_of_engine_protocol) {
    {
        const auto res = wasm::read_request(
            R"({"Black":18446462598732840960,"White":34628173824,"Player":1,"Mode":2,"Depth":0,"TimeTicks":-15005000,"UseOpeningBook":true,"HashBits":20})");
        STELLO_CHECK(res.ok);
        STELLO_CHECK(res.request.board == Board(18446462598732840960ULL, 34628173824ULL));
        STELLO_CHECK(res.request.player == Player::White);
        STELLO_CHECK(res.request.limits == SearchLimits::time_per_game_ms(-1500));
        STELLO_CHECK_EQ(res.request.hash_bits, 20u);
    }
    {
        const auto res = wasm::read_request(
            R"( { "Black" : 1 , "White" : 2, "Player": 0, "Mode": 0, "Depth": 12, "TimeTicks": 0, "Extra": "x" } )");
        STELLO_CHECK(res.ok);
        STELLO_CHECK(res.request.limits == SearchLimits::fixed_depth(12));
        STELLO_CHECK_EQ(res.request.hash_bits, wasm::DEFAULT_HASH_BITS);
    }
    {
        const auto res = wasm::read_request(
            R"({"Black":1,"White":2,"Player":0,"Mode":1,"Depth":0,"TimeTicks":5009999})");
        STELLO_CHECK(res.ok);
        STELLO_CHECK(res.request.limits == SearchLimits::time_per_move_ms(500));
    }
    {
        const auto res = wasm::read_request(
            R"({"Black":1,"White":2,"Player":0,"Mode":3,"Depth":0,"TimeTicks":0,"UseOpeningBook":false})");
        STELLO_CHECK(res.ok);
        STELLO_CHECK(res.request.limits == SearchLimits::solve());
    }
    {
        const auto res = wasm::read_request(
            R"({"Black":1,"White":2,"Player":0,"Mode":3,"Depth":0,"TimeTicks":0,"HashBits":19,"Threads":8})");
        STELLO_CHECK(res.ok);
        STELLO_CHECK(res.request.limits == *SearchLimits::solve().with_threads(8));
    }
}

STELLO_TEST(wasm_protocol_rejects_invalid_requests) {
    constexpr std::string_view kBadRequests[] = {
        "",
        "{",
        "[]",
        R"({"Black":1,"White":2,"Player":0,"Mode":0,"Depth":4}x)",
        R"({"Black":1,"White":3,"Player":0,"Mode":0,"Depth":4})",
        R"({"Black":1,"White":2,"Player":2,"Mode":0,"Depth":4})",
        R"({"Black":1,"White":2,"Player":0,"Mode":4,"Depth":4})",
        R"({"Black":1,"White":2,"Player":0,"Mode":0,"Depth":61})",
        R"({"Black":1,"White":2,"Player":0,"Mode":0,"Depth":4,"HashBits":9})",
        R"({"Black":1,"White":2,"Player":0,"Mode":0,"Depth":4,"Threads":0})",
        R"({"Black":1,"White":2,"Player":0,"Mode":0,"Depth":4,"Threads":257})",
        R"({"Black":-1,"White":2,"Player":0,"Mode":0,"Depth":4})",
        R"({"Black":"1","White":2,"Player":0,"Mode":0,"Depth":4})",
        R"({"White":2,"Player":0,"Mode":0,"Depth":4})",
        R"({"Black\"":1,"White":2,"Player":0,"Mode":0,"Depth":4})",
    };
    for (const auto json : kBadRequests) {
        const auto res = wasm::read_request(json);
        STELLO_CHECK(!res.ok);
        STELLO_CHECK(!res.error.empty());
    }
}

STELLO_TEST(wasm_protocol_writes_results_and_progress) {
    SearchResult result;
    result.mv = Move(Square(37));
    result.score = -12;
    result.kind = ScoreKind::Exact;
    result.depth = 22;
    result.nodes = 123'456'789ULL;
    result.evaluations = 42ULL;
    result.elapsed = std::chrono::microseconds(1'234'567);

    STELLO_CHECK(
        wasm::write_result(result) ==
        R"({"Square":37,"Score":-12,"Kind":3,"Depth":22,"Nodes":123456789,"Evaluations":42,"ElapsedTicks":12345670})");

    SearchResult pass_res = result;
    pass_res.mv = Move::pass();
    STELLO_CHECK(wasm::write_result(pass_res).starts_with(R"({"Square":-1,)"));

    SearchInfo info;
    info.depth = 9;
    info.current_move = Square(19);
    info.best_move = std::nullopt;
    info.score = 150;
    info.kind = ScoreKind::Heuristic;
    info.nodes = 1000ULL;
    info.evaluations = 900ULL;
    info.elapsed = std::chrono::milliseconds(250);

    STELLO_CHECK(
        wasm::write_progress(info) ==
        R"({"Depth":9,"CurrentMove":19,"BestMove":-1,"Score":150,"Kind":1,"Nodes":1000,"Evaluations":900,"ElapsedTicks":2500000})");
}

} // namespace stello::tests
