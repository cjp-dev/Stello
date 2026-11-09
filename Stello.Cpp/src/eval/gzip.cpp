#include "eval/gzip.hpp"

#include <array>
#include <cstdint>

namespace stello::eval::gzip {
namespace {

constexpr uint32_t kMaxBits = 15;
constexpr size_t kMaxLiteralSymbols = 288;
constexpr size_t kMaxDistSymbols = 32;

const std::array<uint32_t, 256>& crc32_table() noexcept {
    static const std::array<uint32_t, 256> kTable = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            }
            t[i] = c;
        }
        return t;
    }();
    return kTable;
}

uint32_t update_crc32(uint32_t crc, std::span<const uint8_t> bytes) noexcept {
    const auto& table = crc32_table();
    uint32_t c = crc ^ 0xFFFF'FFFFU;
    for (uint8_t b : bytes) {
        c = table[(c ^ b) & 0xFFU] ^ (c >> 8);
    }
    return c ^ 0xFFFF'FFFFU;
}

inline uint16_t read_le16(std::span<const uint8_t> s, size_t off) noexcept {
    return static_cast<uint16_t>(s[off]) | (static_cast<uint16_t>(s[off + 1]) << 8);
}

inline uint32_t read_le32(std::span<const uint8_t> s, size_t off) noexcept {
    return static_cast<uint32_t>(s[off])
         | (static_cast<uint32_t>(s[off + 1]) << 8)
         | (static_cast<uint32_t>(s[off + 2]) << 16)
         | (static_cast<uint32_t>(s[off + 3]) << 24);
}

struct HuffmanTable {
    std::array<uint16_t, kMaxBits + 1> count{};
    std::array<uint16_t, kMaxLiteralSymbols> symbols{};
    // 9-bit reversed fast LUT: 0 means empty/slow-path; otherwise ((sym << 4) | len).
    std::array<uint32_t, 512> fast{};

    bool build(std::span<const uint8_t> lengths) noexcept {
        count.fill(0);
        fast.fill(0);

        for (uint8_t len : lengths) {
            if (len > kMaxBits) {
                return false;
            }
            ++count[len];
        }

        if (count[0] == lengths.size()) {
            return true;
        }

        int32_t left = 1;
        for (uint32_t len = 1; len <= kMaxBits; ++len) {
            left <<= 1;
            left -= static_cast<int32_t>(count[len]);
            if (left < 0) {
                return false;
            }
        }

        std::array<uint16_t, kMaxBits + 1> offsets{};
        for (uint32_t len = 1; len < kMaxBits; ++len) {
            offsets[len + 1] = static_cast<uint16_t>(offsets[len] + count[len]);
        }

        for (size_t sym = 0; sym < lengths.size(); ++sym) {
            const uint8_t len = lengths[sym];
            if (len != 0) {
                symbols[offsets[len]++] = static_cast<uint16_t>(sym);
            }
        }

        // Populate 9-bit reversed lookup table for codes up to 9 bits.
        uint32_t code = 0;
        size_t sym_idx = 0;
        for (uint32_t len = 1; len <= 9; ++len) {
            const uint32_t cnt = count[len];
            for (uint32_t i = 0; i < cnt; ++i) {
                uint32_t rev = 0;
                for (uint32_t b = 0; b < len; ++b) {
                    if ((code >> (len - 1U - b)) & 1U) {
                        rev |= (1U << b);
                    }
                }
                const uint32_t packed = (static_cast<uint32_t>(symbols[sym_idx++]) << 4) | len;
                const uint32_t step = 1U << len;
                for (uint32_t idx = rev; idx < 512U; idx += step) {
                    fast[idx] = packed;
                }
                ++code;
            }
            code <<= 1;
        }

        return true;
    }
};

struct BitReader {
    std::span<const uint8_t> data;
    size_t pos{0};
    uint64_t bitbuf{0};
    uint32_t bitcnt{0};

    bool ensure_bits(uint32_t n) noexcept {
        while (bitcnt < n) {
            if (pos >= data.size()) {
                return false;
            }
            bitbuf |= static_cast<uint64_t>(data[pos++]) << bitcnt;
            bitcnt += 8;
        }
        return true;
    }

    bool read_bits(uint32_t n, uint32_t& out) noexcept {
        if (!ensure_bits(n)) {
            return false;
        }
        out = static_cast<uint32_t>(bitbuf & ((uint64_t{1} << n) - 1ULL));
        bitbuf >>= n;
        bitcnt -= n;
        return true;
    }

    void align_to_byte() noexcept {
        const uint32_t skip = bitcnt & 7U;
        bitbuf >>= skip;
        bitcnt -= skip;
        pos -= (bitcnt >> 3);
        bitbuf = 0;
        bitcnt = 0;
    }

    bool decode_symbol(const HuffmanTable& table, uint32_t& sym_out) noexcept {
        if (bitcnt < 9U) {
            while (bitcnt < 15U && pos < data.size()) {
                bitbuf |= static_cast<uint64_t>(data[pos++]) << bitcnt;
                bitcnt += 8;
            }
        }
        if (bitcnt > 0) {
            const uint32_t entry = table.fast[bitbuf & 511ULL];
            if (entry != 0 && (entry & 0xFU) <= bitcnt) {
                const uint32_t len = entry & 0xFU;
                sym_out = entry >> 4;
                bitbuf >>= len;
                bitcnt -= len;
                return true;
            }
        }

        int32_t code = 0;
        int32_t first = 0;
        int32_t index = 0;
        for (uint32_t len = 1; len <= kMaxBits; ++len) {
            if (!ensure_bits(1)) {
                return false;
            }
            code |= static_cast<int32_t>(bitbuf & 1ULL);
            bitbuf >>= 1;
            --bitcnt;

            const int32_t count = static_cast<int32_t>(table.count[len]);
            if (code - count < first) {
                sym_out = table.symbols[static_cast<size_t>(index + (code - first))];
                return true;
            }
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return false;
    }
};

const HuffmanTable& fixed_lit_table() noexcept {
    static const HuffmanTable kTable = [] {
        std::array<uint8_t, 288> lengths{};
        for (size_t i = 0; i <= 143; ++i) lengths[i] = 8;
        for (size_t i = 144; i <= 255; ++i) lengths[i] = 9;
        for (size_t i = 256; i <= 279; ++i) lengths[i] = 7;
        for (size_t i = 280; i <= 287; ++i) lengths[i] = 8;
        HuffmanTable t;
        t.build(lengths);
        return t;
    }();
    return kTable;
}

const HuffmanTable& fixed_dist_table() noexcept {
    static const HuffmanTable kTable = [] {
        std::array<uint8_t, 32> lengths{};
        lengths.fill(5);
        HuffmanTable t;
        t.build(lengths);
        return t;
    }();
    return kTable;
}

constexpr std::array<uint16_t, 29> kLenBase = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
constexpr std::array<uint8_t, 29> kLenExtra = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
constexpr std::array<uint16_t, 30> kDistBase = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
constexpr std::array<uint8_t, 30> kDistExtra = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};
constexpr std::array<uint8_t, 19> kCodeOrder = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

bool decode_huffman_block(
    BitReader& br,
    const HuffmanTable& lit_table,
    const HuffmanTable& dist_table,
    size_t member_start,
    std::vector<uint8_t>& out,
    std::string& error
) {
    while (true) {
        uint32_t sym = 0;
        if (!br.decode_symbol(lit_table, sym)) {
            error = "truncated deflate symbol";
            return false;
        }
        if (sym < 256U) {
            out.push_back(static_cast<uint8_t>(sym));
            continue;
        }
        if (sym == 256U) {
            return true;
        }
        if (sym > 285U) {
            error = "invalid deflate length symbol";
            return false;
        }

        const size_t len_idx = static_cast<size_t>(sym - 257U);
        uint32_t extra = 0;
        if (kLenExtra[len_idx] > 0 && !br.read_bits(kLenExtra[len_idx], extra)) {
            error = "truncated deflate length bits";
            return false;
        }
        const uint32_t match_len = kLenBase[len_idx] + extra;

        uint32_t dist_sym = 0;
        if (!br.decode_symbol(dist_table, dist_sym) || dist_sym >= 30U) {
            error = "invalid deflate distance symbol";
            return false;
        }
        uint32_t dist_extra = 0;
        if (kDistExtra[dist_sym] > 0 && !br.read_bits(kDistExtra[dist_sym], dist_extra)) {
            error = "truncated deflate distance bits";
            return false;
        }
        const size_t distance = static_cast<size_t>(kDistBase[dist_sym] + dist_extra);
        if (distance == 0 || distance > out.size() - member_start) {
            error = "deflate back-reference before start of member";
            return false;
        }

        const size_t dst_start = out.size();
        out.resize(dst_start + match_len);
        for (size_t i = 0; i < match_len; ++i) {
            out[dst_start + i] = out[dst_start - distance + i];
        }
    }
}

bool decode_dynamic_tables(BitReader& br, HuffmanTable& lit_table, HuffmanTable& dist_table, std::string& error) {
    uint32_t hlit_bits = 0, hdist_bits = 0, hclen_bits = 0;
    if (!br.read_bits(5, hlit_bits) || !br.read_bits(5, hdist_bits) || !br.read_bits(4, hclen_bits)) {
        error = "truncated dynamic huffman header";
        return false;
    }
    const size_t hlit = 257U + hlit_bits;
    const size_t hdist = 1U + hdist_bits;
    const size_t hclen = 4U + hclen_bits;
    if (hlit > kMaxLiteralSymbols || hdist > kMaxDistSymbols) {
        error = "invalid dynamic huffman counts";
        return false;
    }

    std::array<uint8_t, 19> code_lengths{};
    for (size_t i = 0; i < hclen; ++i) {
        uint32_t v = 0;
        if (!br.read_bits(3, v)) {
            error = "truncated code length table";
            return false;
        }
        code_lengths[kCodeOrder[i]] = static_cast<uint8_t>(v);
    }

    HuffmanTable code_table;
    if (!code_table.build(code_lengths)) {
        error = "invalid code length huffman tree";
        return false;
    }

    std::array<uint8_t, kMaxLiteralSymbols + kMaxDistSymbols> combined{};
    const size_t total = hlit + hdist;
    size_t idx = 0;
    while (idx < total) {
        uint32_t sym = 0;
        if (!br.decode_symbol(code_table, sym)) {
            error = "truncated dynamic code length entry";
            return false;
        }
        if (sym < 16U) {
            combined[idx++] = static_cast<uint8_t>(sym);
        } else {
            uint8_t value = 0;
            uint32_t repeat = 0;
            uint32_t bits = 0;
            if (sym == 16U) {
                if (idx == 0 || !br.read_bits(2, bits)) {
                    error = "invalid repeat code 16";
                    return false;
                }
                value = combined[idx - 1];
                repeat = 3U + bits;
            } else if (sym == 17U) {
                if (!br.read_bits(3, bits)) {
                    error = "truncated repeat code 17";
                    return false;
                }
                repeat = 3U + bits;
            } else if (sym == 18U) {
                if (!br.read_bits(7, bits)) {
                    error = "truncated repeat code 18";
                    return false;
                }
                repeat = 11U + bits;
            } else {
                error = "invalid code length symbol";
                return false;
            }
            if (idx + repeat > total) {
                error = "code lengths exceed table size";
                return false;
            }
            for (uint32_t r = 0; r < repeat; ++r) {
                combined[idx++] = value;
            }
        }
    }

    if (combined[256] == 0) {
        error = "missing end-of-block symbol in literal tree";
        return false;
    }
    if (!lit_table.build({combined.data(), hlit}) || !dist_table.build({combined.data() + hlit, hdist})) {
        error = "invalid dynamic literal or distance huffman tree";
        return false;
    }
    return true;
}

bool inflate_member(std::span<const uint8_t> input, size_t& pos, std::vector<uint8_t>& out, std::string& error) {
    if (pos + 10 > input.size()) {
        error = "truncated gzip header";
        return false;
    }
    const size_t header_start = pos;
    if (input[pos] != 0x1FU || input[pos + 1] != 0x8BU) {
        error = "invalid gzip magic";
        return false;
    }
    if (input[pos + 2] != 8U) {
        error = "unsupported gzip compression method";
        return false;
    }
    const uint8_t flags = input[pos + 3];
    if ((flags & 0xE0U) != 0U) {
        error = "reserved gzip header flags set";
        return false;
    }
    pos += 10;

    if ((flags & 0x04U) != 0U) { // FEXTRA
        if (pos + 2 > input.size()) {
            error = "truncated gzip extra field";
            return false;
        }
        const uint16_t xlen = read_le16(input, pos);
        pos += 2;
        if (pos + xlen > input.size()) {
            error = "truncated gzip extra data";
            return false;
        }
        pos += xlen;
    }
    if ((flags & 0x08U) != 0U) { // FNAME
        while (pos < input.size() && input[pos] != 0) ++pos;
        if (pos >= input.size()) {
            error = "unterminated gzip file name";
            return false;
        }
        ++pos;
    }
    if ((flags & 0x10U) != 0U) { // FCOMMENT
        while (pos < input.size() && input[pos] != 0) ++pos;
        if (pos >= input.size()) {
            error = "unterminated gzip comment";
            return false;
        }
        ++pos;
    }
    if ((flags & 0x02U) != 0U) { // FHCRC
        if (pos + 2 > input.size()) {
            error = "truncated gzip header crc";
            return false;
        }
        const uint16_t expected_crc16 = read_le16(input, pos);
        const uint32_t header_crc = update_crc32(0, input.subspan(header_start, pos - header_start));
        if (static_cast<uint16_t>(header_crc & 0xFFFFU) != expected_crc16) {
            error = "gzip header crc mismatch";
            return false;
        }
        pos += 2;
    }

    const size_t member_start = out.size();
    BitReader br{input, pos, 0, 0};
    uint32_t bfinal = 0;
    do {
        uint32_t btype = 0;
        if (!br.read_bits(1, bfinal) || !br.read_bits(2, btype)) {
            error = "truncated deflate block header";
            return false;
        }
        if (btype == 0U) {
            br.align_to_byte();
            if (br.pos + 4 > input.size()) {
                error = "truncated stored deflate block";
                return false;
            }
            const uint16_t len = read_le16(input, br.pos);
            const uint16_t nlen = read_le16(input, br.pos + 2);
            br.pos += 4;
            if (static_cast<uint16_t>(len ^ 0xFFFFU) != nlen) {
                error = "stored deflate length mismatch";
                return false;
            }
            if (br.pos + len > input.size()) {
                error = "truncated stored deflate payload";
                return false;
            }
            out.insert(out.end(), input.data() + br.pos, input.data() + br.pos + len);
            br.pos += len;
        } else if (btype == 1U) {
            if (!decode_huffman_block(br, fixed_lit_table(), fixed_dist_table(), member_start, out, error)) {
                return false;
            }
        } else if (btype == 2U) {
            HuffmanTable lit_table;
            HuffmanTable dist_table;
            if (!decode_dynamic_tables(br, lit_table, dist_table, error)) {
                return false;
            }
            if (!decode_huffman_block(br, lit_table, dist_table, member_start, out, error)) {
                return false;
            }
        } else {
            error = "invalid deflate block type";
            return false;
        }
    } while (bfinal == 0U);

    br.align_to_byte();
    pos = br.pos;
    if (pos + 8 > input.size()) {
        error = "truncated gzip trailer";
        return false;
    }
    const uint32_t expected_crc = read_le32(input, pos);
    const uint32_t expected_size = read_le32(input, pos + 4);
    pos += 8;

    const std::span<const uint8_t> member_bytes(out.data() + member_start, out.size() - member_start);
    const uint32_t actual_crc = update_crc32(0, member_bytes);
    if (actual_crc != expected_crc) {
        error = "gzip CRC-32 mismatch";
        return false;
    }
    if (static_cast<uint32_t>(member_bytes.size() & 0xFFFF'FFFFU) != expected_size) {
        error = "gzip ISIZE mismatch";
        return false;
    }
    return true;
}

} // namespace

DecompressResult decompress(std::span<const uint8_t> input) {
    DecompressResult result;
    if (input.empty()) {
        result.error = "empty gzip stream";
        return result;
    }

    size_t pos = 0;
    while (pos < input.size()) {
        if (!inflate_member(input, pos, result.data, result.error)) {
            result.data.clear();
            return result;
        }
    }
    return result;
}

} // namespace stello::eval::gzip
