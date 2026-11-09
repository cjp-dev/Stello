#include "tests/test_harness.hpp"

#include "core/version.hpp"
#include "eval/embedded_eval.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace stello::tests {

STELLO_TEST(phase0_version_and_embedded_eval_smoke) {
    STELLO_CHECK(std::string_view(kEngineVersion) == "3.1.0");
    STELLO_CHECK_EQ(kEngineVersionCode, 30100u);
    STELLO_CHECK_EQ(kAbiVersion, 3u);

    const auto eval_bytes = eval::detail::embedded_eval_bytes();
    STELLO_CHECK_EQ(eval_bytes.size(), size_t{990568});
    STELLO_CHECK_EQ(eval_bytes[0], uint8_t{0x1f});
    STELLO_CHECK_EQ(eval_bytes[1], uint8_t{0x8b});

    STELLO_CHECK(std::filesystem::exists(repo_root() / "Stello.Rust" / "stello-core" / "tests" / "data" / "positions.txt"));
    STELLO_CHECK(std::filesystem::exists(repo_root() / "Stello.Net" / "Stello.Engine.Tests" / "Data" / "stello-v3-signatures.txt"));
}

} // namespace stello::tests

int main(int argc, char* argv[]) {
    const std::string_view filter = (argc >= 2) ? std::string_view(argv[1]) : std::string_view{};
    int passed = 0;
    int failed = 0;
    for (const auto& tc : stello::tests::registry()) {
        if (!filter.empty() && std::string_view(tc.name).find(filter) == std::string_view::npos) {
            continue;
        }
        try {
            tc.fn();
            std::cout << "[PASS] " << tc.name << "\n";
            ++passed;
        } catch (const std::exception& ex) {
            std::cerr << "[FAIL] " << tc.name << ": " << ex.what() << "\n";
            ++failed;
        }
    }
    std::cout << "Summary: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
