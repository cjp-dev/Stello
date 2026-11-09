#include "wasm/protocol.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace stello::wasm {
namespace {

uint64_t duration_to_ticks(Duration d) noexcept {
    if (d <= Duration::zero()) {
        return 0;
    }
    return static_cast<uint64_t>(d.count() / 100);
}

template <typename IntT>
void append_int(std::string& out, IntT value) {
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    (void)ec;
    out.append(buf, static_cast<size_t>(ptr - buf));
}

bool parse_i64(std::string_view text, int64_t& out) noexcept {
    if (text.empty()) {
        return false;
    }
    if (text.front() == '+' && text.size() >= 2) {
        text.remove_prefix(1);
    }
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && ptr == end;
}

bool parse_u64(std::string_view text, uint64_t& out) noexcept {
    if (text.empty() || text.front() == '-') {
        return false;
    }
    if (text.front() == '+' && text.size() >= 2) {
        text.remove_prefix(1);
    }
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && ptr == end;
}

struct JsonValue {
    enum class Kind : uint8_t { Number, Bool, Null, Text };
    Kind kind{Kind::Null};
    std::string_view text{};

    bool as_i64(std::string_view name, int64_t& out, std::string& error) const {
        if (kind != Kind::Number) {
            error = std::string(name) + " is not a number.";
            return false;
        }
        if (!parse_i64(text, out)) {
            error = std::string(name) + " is not an integer: " + std::string(text) + ".";
            return false;
        }
        return true;
    }

    bool as_u64(std::string_view name, uint64_t& out, std::string& error) const {
        if (kind != Kind::Number) {
            error = std::string(name) + " is not a number.";
            return false;
        }
        if (!parse_u64(text, out)) {
            error = std::string(name) + " is not a 64-bit board: " + std::string(text) + ".";
            return false;
        }
        return true;
    }
};

constexpr bool is_ascii_ws(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

constexpr bool is_number_char(char c) noexcept {
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E';
}

class JsonReader {
public:
    explicit JsonReader(std::string_view text) noexcept : text_(text) {}

    bool read_object(std::vector<std::pair<std::string_view, JsonValue>>& members, std::string& error) {
        if (!expect('{', error)) {
            return false;
        }
        if (peek() == '}') {
            ++at_;
        } else {
            for (;;) {
                std::string_view name;
                if (!read_string(name, error)) {
                    return false;
                }
                if (!expect(':', error)) {
                    return false;
                }
                JsonValue val;
                if (!read_value(val, error)) {
                    return false;
                }
                members.emplace_back(name, val);
                const std::optional<char> sep = next();
                if (!sep.has_value()) {
                    error = make_error("',' or '}'");
                    return false;
                }
                if (*sep == ',') {
                    continue;
                }
                if (*sep == '}') {
                    break;
                }
                error = make_error("',' or '}'");
                return false;
            }
        }
        if (peek().has_value()) {
            error = make_error("the end");
            return false;
        }
        return true;
    }

private:
    bool read_value(JsonValue& out, std::string& error) {
        const std::optional<char> c = peek();
        if (!c.has_value()) {
            error = make_error("a value");
            return false;
        }
        if (*c == '"') {
            std::string_view ignored;
            if (!read_string(ignored, error)) {
                return false;
            }
            out = JsonValue{JsonValue::Kind::Text, ignored};
            return true;
        }
        if (*c == 't') {
            if (!read_word("true", error)) return false;
            out = JsonValue{JsonValue::Kind::Bool, {}};
            return true;
        }
        if (*c == 'f') {
            if (!read_word("false", error)) return false;
            out = JsonValue{JsonValue::Kind::Bool, {}};
            return true;
        }
        if (*c == 'n') {
            if (!read_word("null", error)) return false;
            out = JsonValue{JsonValue::Kind::Null, {}};
            return true;
        }
        if (*c == '-' || (*c >= '0' && *c <= '9')) {
            const size_t start = at_;
            while (at_ < text_.size() && is_number_char(text_[at_])) {
                ++at_;
            }
            out = JsonValue{JsonValue::Kind::Number, text_.substr(start, at_ - start)};
            return true;
        }
        error = make_error("a value");
        return false;
    }

    bool read_string(std::string_view& out, std::string& error) {
        if (!expect('"', error)) {
            return false;
        }
        const size_t start = at_;
        const size_t end = text_.find('"', start);
        if (end == std::string_view::npos) {
            error = make_error("'\"'");
            return false;
        }
        const std::string_view slice = text_.substr(start, end - start);
        if (slice.find('\\') != std::string_view::npos) {
            error = make_error("a string without escapes");
            return false;
        }
        at_ = end + 1;
        out = slice;
        return true;
    }

    bool read_word(std::string_view word, std::string& error) {
        if (text_.substr(at_).starts_with(word)) {
            at_ += word.size();
            return true;
        }
        error = make_error(word);
        return false;
    }

    bool expect(char ch, std::string& error) {
        const std::optional<char> c = next();
        if (c.has_value() && *c == ch) {
            return true;
        }
        std::string quoted = "'";
        quoted.push_back(ch);
        quoted.push_back('\'');
        error = make_error(quoted);
        return false;
    }

    std::optional<char> next() noexcept {
        const std::optional<char> c = peek();
        if (c.has_value()) {
            ++at_;
        }
        return c;
    }

    std::optional<char> peek() noexcept {
        while (at_ < text_.size() && is_ascii_ws(text_[at_])) {
            ++at_;
        }
        if (at_ < text_.size()) {
            return text_[at_];
        }
        return std::nullopt;
    }

    std::string make_error(std::string_view expected) const {
        std::string msg = "Not a valid engine request: expected ";
        msg.append(expected);
        msg.append(" at character ");
        append_int(msg, at_);
        msg.push_back('.');
        return msg;
    }

    std::string_view text_;
    size_t at_{0};
};

} // namespace

ReadRequestResult read_request(std::string_view json) {
    ReadRequestResult out;
    std::vector<std::pair<std::string_view, JsonValue>> members;
    JsonReader reader(json);
    if (!reader.read_object(members, out.error)) {
        return out;
    }

    std::optional<uint64_t> black;
    std::optional<uint64_t> white;
    std::optional<int64_t> player_raw;
    std::optional<int64_t> mode_raw;
    int64_t depth_raw = 0;
    int64_t time_ticks = 0;
    int64_t hash_bits_raw = static_cast<int64_t>(DEFAULT_HASH_BITS);
    int64_t threads_raw = 1;

    for (const auto& [name, value] : members) {
        if (name == "Black") {
            uint64_t v = 0;
            if (!value.as_u64(name, v, out.error)) return out;
            black = v;
        } else if (name == "White") {
            uint64_t v = 0;
            if (!value.as_u64(name, v, out.error)) return out;
            white = v;
        } else if (name == "Player") {
            int64_t v = 0;
            if (!value.as_i64(name, v, out.error)) return out;
            player_raw = v;
        } else if (name == "Mode") {
            int64_t v = 0;
            if (!value.as_i64(name, v, out.error)) return out;
            mode_raw = v;
        } else if (name == "Depth") {
            if (!value.as_i64(name, depth_raw, out.error)) return out;
        } else if (name == "TimeTicks") {
            if (!value.as_i64(name, time_ticks, out.error)) return out;
        } else if (name == "HashBits") {
            if (!value.as_i64(name, hash_bits_raw, out.error)) return out;
        } else if (name == "Threads") {
            if (!value.as_i64(name, threads_raw, out.error)) return out;
        }
    }

    if (!black.has_value()) {
        out.error = "The request has no Black.";
        return out;
    }
    if (!white.has_value()) {
        out.error = "The request has no White.";
        return out;
    }
    if ((*black & *white) != 0ULL) {
        out.error = "A square is both black and white.";
        return out;
    }
    if (!player_raw.has_value()) {
        out.error = "The request has no Player.";
        return out;
    }

    Player player = Player::Black;
    if (*player_raw == 0) {
        player = Player::Black;
    } else if (*player_raw == 1) {
        player = Player::White;
    } else {
        out.error = "Not a player: ";
        append_int(out.error, *player_raw);
        out.error.push_back('.');
        return out;
    }

    if (!mode_raw.has_value()) {
        out.error = "The request has no Mode.";
        return out;
    }

    TimeControlMode mode = TimeControlMode::FixedDepth;
    switch (*mode_raw) {
        case 0: mode = TimeControlMode::FixedDepth; break;
        case 1: mode = TimeControlMode::TimePerMove; break;
        case 2: mode = TimeControlMode::TimePerGame; break;
        case 3: mode = TimeControlMode::Solve; break;
        default:
            out.error = "Not a time control: ";
            append_int(out.error, *mode_raw);
            out.error.push_back('.');
            return out;
    }

    if (depth_raw < (std::numeric_limits<int32_t>::min)() || depth_raw > (std::numeric_limits<int32_t>::max)()) {
        out.error = "Not a depth: ";
        append_int(out.error, depth_raw);
        out.error.push_back('.');
        return out;
    }

    const int64_t time_ms = time_ticks / 10'000LL;
    const auto base_limits = SearchLimits::from_parts(mode, static_cast<int32_t>(depth_raw), time_ms);
    if (!base_limits.has_value()) {
        out.error = "The limits are not valid.";
        return out;
    }

    std::optional<SearchLimits> limits;
    if (threads_raw >= 1 && threads_raw <= static_cast<int64_t>(SearchLimits::MAX_THREADS)) {
        limits = base_limits->with_threads(static_cast<uint32_t>(threads_raw));
    }
    if (!limits.has_value()) {
        out.error = "Threads must be 1-";
        append_int(out.error, SearchLimits::MAX_THREADS);
        out.error.append(", not ");
        append_int(out.error, threads_raw);
        out.error.push_back('.');
        return out;
    }

    if (hash_bits_raw < 10 || hash_bits_raw > 26) {
        out.error = "Hash bits must be 10-26, not ";
        append_int(out.error, hash_bits_raw);
        out.error.push_back('.');
        return out;
    }

    out.ok = true;
    out.request = MoveRequest{Board(*black, *white), player, *limits, static_cast<uint32_t>(hash_bits_raw)};
    return out;
}

std::string write_result(const SearchResult& result) {
    const int32_t square = result.mv.square.has_value() ? static_cast<int32_t>(result.mv.square->index()) : -1;
    std::string json;
    json.reserve(160);
    json.append("{\"Square\":");
    append_int(json, square);
    json.append(",\"Score\":");
    append_int(json, result.score);
    json.append(",\"Kind\":");
    append_int(json, static_cast<uint32_t>(result.kind));
    json.append(",\"Depth\":");
    append_int(json, result.depth);
    json.append(",\"Nodes\":");
    append_int(json, result.nodes);
    json.append(",\"Evaluations\":");
    append_int(json, result.evaluations);
    json.append(",\"ElapsedTicks\":");
    append_int(json, duration_to_ticks(result.elapsed));
    json.push_back('}');
    return json;
}

std::string write_progress(const SearchInfo& info) {
    const int32_t best = info.best_move.has_value() ? static_cast<int32_t>(info.best_move->index()) : -1;
    std::string json;
    json.reserve(160);
    json.append("{\"Depth\":");
    append_int(json, info.depth);
    json.append(",\"CurrentMove\":");
    append_int(json, static_cast<uint32_t>(info.current_move.index()));
    json.append(",\"BestMove\":");
    append_int(json, best);
    json.append(",\"Score\":");
    append_int(json, info.score);
    json.append(",\"Kind\":");
    append_int(json, static_cast<uint32_t>(info.kind));
    json.append(",\"Nodes\":");
    append_int(json, info.nodes);
    json.append(",\"Evaluations\":");
    append_int(json, info.evaluations);
    json.append(",\"ElapsedTicks\":");
    append_int(json, duration_to_ticks(info.elapsed));
    json.push_back('}');
    return json;
}

} // namespace stello::wasm
