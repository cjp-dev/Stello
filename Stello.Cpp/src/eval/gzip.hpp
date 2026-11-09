#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace stello::eval::gzip {

struct DecompressResult {
    std::vector<uint8_t> data;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

/// Decompresses a single- or multi-member RFC 1952 gzip stream, verifying CRC-32 and ISIZE trailers.
DecompressResult decompress(std::span<const uint8_t> input);

} // namespace stello::eval::gzip
