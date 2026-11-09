#pragma once

#include <cstdint>
#include <span>

namespace stello::eval::detail {

/// Bytes of the built-in gzip-compressed `stello-eval.bin` file generated at build time.
std::span<const uint8_t> embedded_eval_bytes() noexcept;

} // namespace stello::eval::detail
