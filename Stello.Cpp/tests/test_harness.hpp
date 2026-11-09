#pragma once

#include <cstdint>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace stello::tests {

struct TestFailure final : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

inline int register_test(const char* name, void (*fn)()) {
    registry().push_back({name, fn});
    return 0;
}

inline std::filesystem::path repo_root() {
    return std::filesystem::path(STELLO_REPO_ROOT);
}

inline void check_true(bool ok, const char* file, int line, const char* expr) {
    if (!ok) {
        std::ostringstream oss;
        oss << file << ":" << line << ": check failed: " << expr;
        throw TestFailure(oss.str());
    }
}

inline void check_true_msg(bool ok, const char* file, int line, const char* expr, const std::string& msg) {
    if (!ok) {
        std::ostringstream oss;
        oss << file << ":" << line << ": check failed: " << expr << " [" << msg << "]";
        throw TestFailure(oss.str());
    }
}

template <typename T>
inline void format_value(std::ostringstream& oss, const T& value) {
    if constexpr (std::is_same_v<T, uint8_t>) {
        oss << static_cast<unsigned>(value);
    } else if constexpr (std::is_same_v<T, int8_t>) {
        oss << static_cast<int>(value);
    } else {
        oss << value;
    }
}

template <typename A, typename B>
inline void check_eq(const A& actual, const B& expected, const char* file, int line, const char* lhs, const char* rhs) {
    if (!(actual == expected)) {
        std::ostringstream oss;
        oss << file << ":" << line << ": " << lhs << " (";
        format_value(oss, actual);
        oss << ") != " << rhs << " (";
        format_value(oss, expected);
        oss << ")";
        throw TestFailure(oss.str());
    }
}

template <typename A, typename B>
inline void check_eq_msg(
    const A& actual,
    const B& expected,
    const char* file,
    int line,
    const char* lhs,
    const char* rhs,
    const std::string& msg
) {
    if (!(actual == expected)) {
        std::ostringstream oss;
        oss << file << ":" << line << ": " << lhs << " (";
        format_value(oss, actual);
        oss << ") != " << rhs << " (";
        format_value(oss, expected);
        oss << ") [" << msg << "]";
        throw TestFailure(oss.str());
    }
}

#define STELLO_TEST_CONCAT_INNER(a, b) a##b
#define STELLO_TEST_CONCAT(a, b) STELLO_TEST_CONCAT_INNER(a, b)

#define STELLO_TEST(test_name)                                                     \
    static void test_name();                                                       \
    static const int STELLO_TEST_CONCAT(stello_reg_, __LINE__) =                   \
        ::stello::tests::register_test(#test_name, &test_name);                    \
    static void test_name()

#define STELLO_CHECK(expr) \
    ::stello::tests::check_true(static_cast<bool>(expr), __FILE__, __LINE__, #expr)

#define STELLO_CHECK_MSG(expr, msg) \
    ::stello::tests::check_true_msg(static_cast<bool>(expr), __FILE__, __LINE__, #expr, (msg))

#define STELLO_CHECK_EQ(actual, expected) \
    ::stello::tests::check_eq((actual), (expected), __FILE__, __LINE__, #actual, #expected)

#define STELLO_CHECK_EQ_MSG(actual, expected, msg) \
    ::stello::tests::check_eq_msg((actual), (expected), __FILE__, __LINE__, #actual, #expected, (msg))

} // namespace stello::tests
