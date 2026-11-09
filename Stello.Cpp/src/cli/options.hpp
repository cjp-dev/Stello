#pragma once

#include <algorithm>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stello::cli {

struct Options {
    std::vector<std::pair<std::string, std::string>> named;
    std::vector<std::string> positional;

    static std::optional<Options> parse(
        std::span<const std::string_view> args,
        std::span<const std::string_view> allowed,
        std::string& error_out
    ) {
        Options opts;
        for (size_t i = 0; i < args.size(); ++i) {
            const std::string_view arg = args[i];
            if (arg.starts_with("--")) {
                const std::string_view name = arg.substr(2);
                if (std::find(allowed.begin(), allowed.end(), name) == allowed.end()) {
                    error_out = "Unknown option --" + std::string(name) + ".";
                    return std::nullopt;
                }
                if (i + 1 >= args.size()) {
                    error_out = "--" + std::string(name) + " needs one value.";
                    return std::nullopt;
                }
                opts.named.emplace_back(std::string(name), std::string(args[++i]));
            } else {
                opts.positional.emplace_back(arg);
            }
        }
        return opts;
    }

    std::optional<std::string_view> text(std::string_view name) const noexcept {
        for (const auto& [k, v] : named) {
            if (k == name) {
                return std::string_view(v);
            }
        }
        return std::nullopt;
    }
};

inline std::optional<std::filesystem::path> repository_folder() {
    std::error_code ec;
    auto cur = std::filesystem::current_path(ec);
    if (ec) {
        return std::nullopt;
    }
    while (!cur.empty()) {
        if (std::filesystem::exists(cur / "Stello.Net" / "Stello.Net.slnx", ec)) {
            return cur;
        }
        const auto parent = cur.parent_path();
        if (parent == cur) {
            break;
        }
        cur = parent;
    }
    return std::nullopt;
}

} // namespace stello::cli
