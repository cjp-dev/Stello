#include "eval/patterns.hpp"

#include "core/compiler.hpp"
#include "core/cpu.hpp"
#include "core/square.hpp"
#include "eval/symmetry.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <map>
#include <unordered_set>

namespace stello::eval {
namespace {

constexpr uint64_t kDiagonalMultiplier = 0x0101'0101'0101'0101ULL;

constexpr uint32_t row_of(uint32_t s) noexcept { return s >> 3; }
constexpr uint32_t column_of(uint32_t s) noexcept { return s & 7U; }

constexpr uint32_t pow3(uint32_t exp) noexcept {
    uint32_t p = 1;
    for (uint32_t i = 0; i < exp; ++i) p *= 3U;
    return p;
}

std::vector<PatternShape::Part> make_parts(std::span<const uint32_t> squares) {
    const uint32_t first = squares[0];
    bool same_row = true;
    bool anti_diag = true;
    bool diag = true;
    for (uint32_t s : squares) {
        if (row_of(s) != row_of(first)) same_row = false;
        if (column_of(s) + row_of(s) != column_of(first) + row_of(first)) anti_diag = false;
        if (static_cast<int32_t>(column_of(s)) - static_cast<int32_t>(row_of(s)) !=
            static_cast<int32_t>(column_of(first)) - static_cast<int32_t>(row_of(first))) {
            diag = false;
        }
    }

    std::vector<std::vector<uint32_t>> groups;
    if (!same_row && (anti_diag || diag)) {
        groups.emplace_back(squares.begin(), squares.end());
    } else {
        for (uint32_t sq : squares) {
            auto it = std::find_if(groups.begin(), groups.end(), [&](const auto& g) {
                return row_of(g[0]) == row_of(sq);
            });
            if (it != groups.end()) {
                it->push_back(sq);
            } else {
                groups.push_back({sq});
            }
        }
    }

    std::vector<PatternShape::Part> parts;
    parts.reserve(groups.size());
    for (const auto& group : groups) {
        const bool is_row = std::all_of(group.begin(), group.end(), [&](uint32_t s) {
            return row_of(s) == row_of(group[0]);
        });
        uint64_t mask = 0;
        for (uint32_t s : group) mask |= (uint64_t{1} << s);

        PatternShape::Part part{};
        part.mask = mask;
        part.multiplier = is_row ? 1ULL : kDiagonalMultiplier;
        part.shift = is_row ? (8U * row_of(group[0])) : 56U;

#ifndef NDEBUG
        std::vector<uint32_t> bit_of;
        std::unordered_set<uint32_t> distinct_bits;
        bit_of.reserve(group.size());
        for (uint32_t sq : group) {
            const size_t value = part.extract(uint64_t{1} << sq);
            assert(std::popcount(value) == 1);
            const uint32_t bit = static_cast<uint32_t>(std::countr_zero(value));
            bit_of.push_back(bit);
            distinct_bits.insert(bit);
        }
        assert(distinct_bits.size() == bit_of.size());
#endif

        const uint32_t limit = uint32_t{1} << group.size();
        for (uint32_t subset = 0; subset < limit; ++subset) {
            uint64_t discs = 0;
#ifndef NDEBUG
            size_t expected = 0;
#endif
            uint32_t digits = 0;
            for (size_t i = 0; i < group.size(); ++i) {
                if ((subset & (uint32_t{1} << i)) != 0) {
                    const uint32_t sq = group[i];
                    discs |= uint64_t{1} << sq;
#ifndef NDEBUG
                    expected |= size_t{1} << bit_of[i];
#endif
                    const auto pos_it = std::find(squares.begin(), squares.end(), sq);
                    const uint32_t position = static_cast<uint32_t>(pos_it - squares.begin());
                    digits += pow3(position);
                }
            }
            const size_t extracted = part.extract(discs);
            assert(extracted == expected);
            part.digits[extracted] = static_cast<uint16_t>(digits);
        }
        parts.push_back(part);
    }

    return parts;
}

std::vector<size_t> find_placements(std::span<const uint32_t> squares) {
    std::unordered_set<uint64_t> seen;
    std::vector<size_t> placements;
    for (size_t sym = 0; sym < symmetry::COUNT; ++sym) {
        uint64_t covered = 0;
        for (uint32_t sq = 0; sq < 64; ++sq) {
            const uint32_t mapped = symmetry::map_square(sq, sym);
            if (std::find(squares.begin(), squares.end(), mapped) != squares.end()) {
                covered |= uint64_t{1} << sq;
            }
        }
        if (seen.insert(covered).second) {
            placements.push_back(sym);
        }
    }
    return placements;
}

void make_fold(
    std::span<const uint32_t> squares,
    size_t size,
    std::vector<uint32_t>& fold_out,
    std::vector<uint32_t>& reps_out
) {
    std::vector<std::vector<size_t>> permutations;
    for (size_t sym = 1; sym < symmetry::COUNT; ++sym) {
        std::vector<size_t> mapped;
        bool valid = true;
        for (uint32_t sq : squares) {
            const uint32_t target = symmetry::map_square(sq, sym);
            const auto it = std::find(squares.begin(), squares.end(), target);
            if (it == squares.end()) {
                valid = false;
                break;
            }
            mapped.push_back(static_cast<size_t>(it - squares.begin()));
        }
        if (valid) {
            permutations.push_back(std::move(mapped));
        }
    }

    fold_out.resize(size);
    std::vector<size_t> digits(squares.size());
    for (size_t index = 0; index < size; ++index) {
        size_t rest = index;
        for (size_t i = 0; i < squares.size(); ++i) {
            digits[i] = rest % 3U;
            rest /= 3U;
        }

        size_t smallest = index;
        for (const auto& perm : permutations) {
            size_t other = 0;
            size_t power = 1;
            for (size_t i = 0; i < squares.size(); ++i) {
                const auto from_it = std::find(perm.begin(), perm.end(), i);
                const size_t from = static_cast<size_t>(from_it - perm.begin());
                other += digits[from] * power;
                power *= 3U;
            }
            if (other < smallest) {
                smallest = other;
            }
        }
        fold_out[index] = static_cast<uint32_t>(smallest);
    }

    reps_out.clear();
    for (size_t i = 0; i < size; ++i) {
        if (fold_out[i] == i) {
            reps_out.push_back(static_cast<uint32_t>(i));
        }
    }
}

std::vector<std::vector<uint32_t>> direct_groups(std::span<const uint32_t> original) {
    std::vector<std::vector<uint32_t>> across;
    for (uint32_t sq : original) {
        auto it = std::find_if(across.begin(), across.end(), [&](const auto& g) {
            return std::all_of(g.begin(), g.end(), [&](uint32_t s) { return column_of(s) != column_of(sq); });
        });
        if (it != across.end()) {
            it->push_back(sq);
        } else {
            across.push_back({sq});
        }
    }

    std::vector<std::vector<uint32_t>> columns;
    for (uint32_t sq : original) {
        auto it = std::find_if(columns.begin(), columns.end(), [&](const auto& g) {
            return column_of(g[0]) == column_of(sq);
        });
        if (it != columns.end()) {
            it->push_back(sq);
        } else {
            columns.push_back({sq});
        }
    }

    return columns.size() < across.size() ? columns : across;
}

void line_extraction(std::span<const uint32_t> group, uint64_t& mask_out, uint64_t& mult_out, uint32_t& shift_out) {
    uint64_t mask = 0;
    for (uint32_t sq : group) mask |= (uint64_t{1} << sq);

    const uint32_t first = group[0];
    const bool same_col = group.size() > 1 && std::all_of(group.begin(), group.end(), [&](uint32_t s) {
        return column_of(s) == column_of(first);
    });
    const bool same_row = std::all_of(group.begin(), group.end(), [&](uint32_t s) {
        return row_of(s) == row_of(first);
    });

    uint64_t mult = kDiagonalMultiplier;
    uint32_t shift = 56;
    if (same_col) {
        const uint32_t c = column_of(first);
        mult = 0;
        for (uint32_t r = 0; r < 8; ++r) {
            mult |= uint64_t{1} << (56U - c - 7U * r);
        }
        shift = 56;
    } else if (same_row) {
        mult = 1;
        shift = 8U * row_of(first);
    }

#ifndef NDEBUG
    const auto extract = [&](uint64_t discs) noexcept {
        return (((discs & mask) * mult) >> shift) & 0xFFULL;
    };

    std::vector<uint64_t> bits;
    bits.reserve(group.size());
    for (uint32_t sq : group) {
        const uint64_t b = extract(uint64_t{1} << sq);
        assert(std::popcount(b) == 1);
        bits.push_back(b);
    }

    const uint32_t subsets = uint32_t{1} << group.size();
    for (uint32_t subset = 0; subset < subsets; ++subset) {
        uint64_t discs = 0;
        uint64_t expected = 0;
        for (size_t i = 0; i < group.size(); ++i) {
            if ((subset & (uint32_t{1} << i)) != 0) {
                discs |= uint64_t{1} << group[i];
                expected |= bits[i];
            }
        }
        assert(extract(discs) == expected);
    }
#endif

    mask_out = mask;
    mult_out = mult;
    shift_out = shift;
}

PatternShape shape_from_text(std::string name, std::string_view text) {
    std::vector<uint32_t> squares;
    size_t pos = 0;
    while (pos < text.size()) {
        while (pos < text.size() && text[pos] == ' ') ++pos;
        if (pos >= text.size()) break;
        size_t end = pos;
        while (end < text.size() && text[end] != ' ') ++end;
        const auto sq = Square::parse(text.substr(pos, end - pos));
        assert(sq.has_value());
        squares.push_back(sq->index());
        pos = end;
    }
    return PatternShape(std::move(name), squares);
}

std::vector<PatternShape> classic11_shapes() {
    std::vector<PatternShape> shapes;
    shapes.push_back(PatternShape("row2", std::array<uint32_t, 8>{8, 9, 10, 11, 12, 13, 14, 15}));
    shapes.push_back(PatternShape("row3", std::array<uint32_t, 8>{16, 17, 18, 19, 20, 21, 22, 23}));
    shapes.push_back(PatternShape("row4", std::array<uint32_t, 8>{24, 25, 26, 27, 28, 29, 30, 31}));
    shapes.push_back(PatternShape("diagonal4", std::array<uint32_t, 4>{3, 10, 17, 24}));
    shapes.push_back(PatternShape("diagonal5", std::array<uint32_t, 5>{4, 11, 18, 25, 32}));
    shapes.push_back(PatternShape("diagonal6", std::array<uint32_t, 6>{5, 12, 19, 26, 33, 40}));
    shapes.push_back(PatternShape("diagonal7", std::array<uint32_t, 7>{6, 13, 20, 27, 34, 41, 48}));
    shapes.push_back(PatternShape("diagonal8", std::array<uint32_t, 8>{7, 14, 21, 28, 35, 42, 49, 56}));
    shapes.push_back(PatternShape("edge+2x", std::array<uint32_t, 10>{0, 1, 2, 3, 4, 5, 6, 7, 9, 14}));
    shapes.push_back(PatternShape("corner2x5", std::array<uint32_t, 10>{0, 1, 2, 3, 4, 8, 9, 10, 11, 12}));
    shapes.push_back(PatternShape("corner3x3", std::array<uint32_t, 9>{0, 1, 2, 8, 9, 10, 16, 17, 18}));
    return shapes;
}

} // namespace

PatternShape::PatternShape(std::string name, std::span<const uint32_t> squares)
    : name_(std::move(name)), squares_(squares.begin(), squares.end()) {
    assert(squares_.size() <= 10);
    std::unordered_set<uint32_t> distinct(squares_.begin(), squares_.end());
    assert(distinct.size() == squares_.size());
    assert(std::all_of(squares_.begin(), squares_.end(), [](uint32_t s) { return s < 64U; }));

    size_ = pow3(static_cast<uint32_t>(squares_.size()));
    make_fold(squares_, size_, fold_, representatives_);
    parts_ = make_parts(squares_);
    placements_ = find_placements(squares_);
}

uint32_t PatternShape::index(uint64_t own, uint64_t opponent) const noexcept {
    uint32_t sum = 0;
    for (const auto& part : parts_) {
        sum += part.index(own, opponent);
    }
    return sum;
}

uint32_t PatternShape::slow_index(uint64_t own, uint64_t opponent) const noexcept {
    uint32_t idx = 0;
    uint32_t power = 1;
    for (uint32_t sq : squares_) {
        const uint64_t bit = uint64_t{1} << sq;
        if ((own & bit) != 0) {
            idx += power;
        } else if ((opponent & bit) != 0) {
            idx += 2U * power;
        }
        power *= 3U;
    }
    return idx;
}

namespace detail {

Direct Direct::build(std::span<const PatternShape> shapes, std::span<const Feature> features) {
    Direct direct;
    std::map<std::vector<uint16_t>, uint32_t> digit_tables;
    std::map<std::vector<uint16_t>, uint32_t> pext_tables;

    const auto share = [](std::vector<uint16_t>& pool,
                          std::map<std::vector<uint16_t>, uint32_t>& tables,
                          std::vector<uint16_t> table) -> uint32_t {
        auto it = tables.find(table);
        if (it != tables.end()) {
            return it->second;
        }
        const uint32_t start = static_cast<uint32_t>(pool.size());
        pool.insert(pool.end(), table.begin(), table.end());
        tables.emplace(std::move(table), start);
        return start;
    };

    for (const auto& feature : features) {
        const auto& shape = shapes[feature.shape];
        const auto shape_squares = shape.squares();

        std::vector<uint32_t> original;
        original.reserve(shape_squares.size());
        for (uint32_t target : shape_squares) {
            uint32_t found = 64;
            for (uint32_t q = 0; q < 64; ++q) {
                if (symmetry::map_square(q, feature.symmetry) == target) {
                    found = q;
                    break;
                }
            }
            assert(found < 64);
            original.push_back(found);
        }

        const auto power_of = [&](uint32_t sq) -> uint16_t {
            const auto it = std::find(original.begin(), original.end(), sq);
            return static_cast<uint16_t>(pow3(static_cast<uint32_t>(it - original.begin())));
        };

        auto groups = direct_groups(original);
        size_t at = direct.groups_.size();
        for (size_t i = 0; i < direct.groups_.size(); ++i) {
            if (direct.groups_[i].count == groups.size()) {
                at = i;
                break;
            }
        }
        if (at == direct.groups_.size()) {
            direct.groups_.push_back(PartGroup{groups.size(), {}, {}});
        }
        direct.groups_[at].offsets.push_back(feature.offset);

        for (const auto& group : groups) {
            uint64_t mask = 0, multiplier = 0;
            uint32_t shift = 0;
            line_extraction(group, mask, multiplier, shift);

            std::vector<uint16_t> table(256, 0);
            const uint32_t subsets = uint32_t{1} << group.size();
            for (uint32_t subset = 0; subset < subsets; ++subset) {
                uint64_t discs = 0;
                uint16_t digits = 0;
                for (size_t i = 0; i < group.size(); ++i) {
                    if ((subset & (uint32_t{1} << i)) != 0) {
                        const uint32_t sq = group[i];
                        discs |= uint64_t{1} << sq;
                        digits = static_cast<uint16_t>(digits + power_of(sq));
                    }
                }
                const size_t extracted = static_cast<size_t>((((discs & mask) * multiplier) >> shift) & 0xFFULL);
                table[extracted] = digits;
            }

            const uint32_t table_offset = share(direct.digits_, digit_tables, std::move(table));
            direct.groups_[at].parts.push_back(DirectPart{mask, multiplier, shift, table_offset});
        }

        // BMI2 pext table: bit k of the extracted mask is the k-th lowest square.
        uint64_t pext_mask = 0;
        for (uint32_t q : original) pext_mask |= (uint64_t{1} << q);
        std::vector<uint32_t> sorted = original;
        std::sort(sorted.begin(), sorted.end());
        const size_t table_len = size_t{1} << sorted.size();
        std::vector<uint16_t> ptable(table_len, 0);
        for (size_t bits = 0; bits < table_len; ++bits) {
            uint16_t sum = 0;
            for (size_t k = 0; k < sorted.size(); ++k) {
                if ((bits & (size_t{1} << k)) != 0) {
                    sum = static_cast<uint16_t>(sum + power_of(sorted[k]));
                }
            }
            ptable[bits] = sum;
        }
        const uint32_t pext_table_offset = share(direct.pext_tables_, pext_tables, std::move(ptable));
        direct.pext_.push_back(PextFeature{pext_mask, pext_table_offset, feature.offset});
    }

    return direct;
}

STELLO_FORCEINLINE uint32_t Direct::part_digits(const DirectPart& part, uint64_t own, uint64_t opponent) const noexcept {
    const uint64_t own_byte = (((own & part.mask) * part.multiplier) >> part.shift) & 0xFFULL;
    const uint64_t opp_byte = (((opponent & part.mask) * part.multiplier) >> part.shift) & 0xFFULL;
    const uint16_t* tbl = digits_.data() + part.table;
    return static_cast<uint32_t>(tbl[own_byte]) + 2U * static_cast<uint32_t>(tbl[opp_byte]);
}

template <size_t N>
STELLO_FORCEINLINE int32_t Direct::sum_group(
    const PartGroup& group,
    uint64_t own,
    uint64_t opponent,
    const int16_t* weights
) const noexcept {
    int32_t sum = 0;
    const size_t n_features = group.offsets.size();
    const DirectPart* p = group.parts.data();
    const uint32_t* offsets = group.offsets.data();
    for (size_t f = 0; f < n_features; ++f) {
        uint32_t idx = offsets[f];
        for (size_t k = 0; k < N; ++k) {
            idx += part_digits(p[k], own, opponent);
        }
        p += N;
        sum += weights[idx];
    }
    return sum;
}

STELLO_NOINLINE int32_t Direct::sum_any(
    const PartGroup& group,
    uint64_t own,
    uint64_t opponent,
    const int16_t* weights
) const noexcept {
    int32_t sum = 0;
    const size_t n_features = group.offsets.size();
    const size_t cnt = group.count;
    for (size_t f = 0; f < n_features; ++f) {
        uint32_t idx = group.offsets[f];
        const size_t base = f * cnt;
        for (size_t k = 0; k < cnt; ++k) {
            idx += part_digits(group.parts[base + k], own, opponent);
        }
        sum += weights[idx];
    }
    return sum;
}

int32_t Direct::sum_portable(uint64_t own, uint64_t opponent, std::span<const int16_t> weights) const noexcept {
    int32_t sum = 0;
    const int16_t* w = weights.data();
    for (const auto& group : groups_) {
        switch (group.count) {
            case 1: sum += sum_group<1>(group, own, opponent, w); break;
            case 2: sum += sum_group<2>(group, own, opponent, w); break;
            case 3: sum += sum_group<3>(group, own, opponent, w); break;
            case 4: sum += sum_group<4>(group, own, opponent, w); break;
            default: sum += sum_any(group, own, opponent, w); break;
        }
    }
    return sum;
}

} // namespace detail

PatternSet::PatternSet(std::string name, std::vector<PatternShape> shapes)
    : name_(std::move(name)), shapes_(std::move(shapes)) {
    assert(!shapes_.empty());
    offsets_.assign(shapes_.size(), 0);
    for (size_t i = 1; i < shapes_.size(); ++i) {
        offsets_[i] = offsets_[i - 1] + shapes_[i - 1].size();
    }
    raw_size_ = offsets_.back() + shapes_.back().size();

    for (size_t i = 0; i < shapes_.size(); ++i) {
        const auto& shape = shapes_[i];
        const auto shape_parts = shape.parts();
        for (size_t sym : shape.placements()) {
            const size_t start = parts_.size();
            parts_.insert(parts_.end(), shape_parts.begin(), shape_parts.end());
            features_.push_back(detail::Feature{
                i,
                sym,
                static_cast<uint32_t>(offsets_[i]),
                start,
                parts_.size(),
            });
        }
    }

    for (const auto& f : features_) {
        symmetries_.push_back(f.symmetry);
    }
    std::sort(symmetries_.begin(), symmetries_.end());
    symmetries_.erase(std::unique(symmetries_.begin(), symmetries_.end()), symmetries_.end());

    direct_ = detail::Direct::build(shapes_, features_);
}

const PatternSet& PatternSet::classic11() {
    static const PatternSet kSet("classic11", classic11_shapes());
    return kSet;
}

const PatternSet& PatternSet::plus15() {
    static const PatternSet kSet = [] {
        auto shapes = classic11_shapes();
        shapes.push_back(shape_from_text("triangle", "a1 b1 c1 d1 a2 b2 c2 a3 b3 a4"));
        shapes.push_back(shape_from_text("corner+block", "a1 c1 d1 e1 f1 h1 c2 d2 e2 f2"));
        shapes.push_back(shape_from_text("cross", "a1 b2 c3 d4 b1 c2 d3 a2 b3 c4"));
        shapes.push_back(shape_from_text("edge+y", "c2 a1 b1 c1 d1 e1 f1 g1 h1 f2"));
        return PatternSet("plus15", std::move(shapes));
    }();
    return kSet;
}

const PatternSet& PatternSet::egaroucid16() {
    static const PatternSet kSet = [] {
        std::vector<PatternShape> shapes;
        shapes.push_back(shape_from_text("hv2", "a2 b2 c2 d2 e2 f2 g2 h2"));
        shapes.push_back(shape_from_text("d6+2c+x", "b1 c1 d2 e3 g2 f4 g5 h6 h7"));
        shapes.push_back(shape_from_text("hv3", "a3 b3 c3 d3 e3 f3 g3 h3"));
        shapes.push_back(shape_from_text("d7+2corner", "a1 b1 c2 d3 e4 f5 g6 h7 h8"));
        shapes.push_back(shape_from_text("hv4", "a4 b4 c4 d4 e4 f4 g4 h4"));
        shapes.push_back(shape_from_text("corner9", "a1 b1 c1 a2 b2 c2 a3 b3 c3"));
        shapes.push_back(shape_from_text("d5+2x", "b2 d1 e2 f3 g4 h5 g7"));
        shapes.push_back(shape_from_text("d8+2c", "a1 b2 c3 d4 e5 f6 g7 h8 a2 b1"));
        shapes.push_back(shape_from_text("edge+2x", "b2 a1 b1 c1 d1 e1 f1 g1 h1 g2"));
        shapes.push_back(shape_from_text("triangle", "a1 b1 c1 d1 a2 b2 c2 a3 b3 a4"));
        shapes.push_back(shape_from_text("corner+block", "a1 c1 d1 e1 f1 h1 c2 d2 e2 f2"));
        shapes.push_back(shape_from_text("cross", "a1 b2 c3 d4 b1 c2 d3 a2 b3 c4"));
        shapes.push_back(shape_from_text("edge+y", "c2 a1 b1 c1 d1 e1 f1 g1 h1 f2"));
        shapes.push_back(shape_from_text("narrow-triangle", "a1 b1 c1 d1 e1 a2 b2 a3 a4 a5"));
        shapes.push_back(shape_from_text("fish", "a1 b1 a2 b2 c2 d2 b3 c3 b4 d4"));
        shapes.push_back(shape_from_text("anvil", "c6 d6 d7 d8 c8 f8 e8 e7 e6 f6"));
        return PatternSet("egaroucid16", std::move(shapes));
    }();
    return kSet;
}

const PatternSet* PatternSet::by_name(std::string_view name) noexcept {
    if (name == "classic11") return &classic11();
    if (name == "plus15") return &plus15();
    if (name == "egaroucid16") return &egaroucid16();
    return nullptr;
}

void PatternSet::indices(uint64_t own, uint64_t opponent, std::span<uint32_t> out) const noexcept {
    assert(out.size() >= features_.size());
    std::array<uint64_t, symmetry::COUNT> turned_own{};
    std::array<uint64_t, symmetry::COUNT> turned_opp{};
    for (size_t s : symmetries_) {
        turned_own[s] = symmetry::apply(own, s);
        turned_opp[s] = symmetry::apply(opponent, s);
    }

    for (size_t f = 0; f < features_.size(); ++f) {
        const auto& feat = features_[f];
        const uint64_t o = turned_own[feat.symmetry];
        const uint64_t p = turned_opp[feat.symmetry];
        uint32_t sum = feat.offset;
        for (size_t i = feat.part_begin; i < feat.part_end; ++i) {
            sum += parts_[i].index(o, p);
        }
        out[f] = sum;
    }
}

int32_t PatternSet::sum_weights(uint64_t own, uint64_t opponent, std::span<const int16_t> weights) const noexcept {
    assert(weights.size() == raw_size_);
#if (defined(__x86_64__) || defined(_M_X64)) && !defined(__EMSCRIPTEN__)
    if (cpu::fast_pext()) {
        return direct_.sum_pext(own, opponent, weights.data());
    }
#endif
    return direct_.sum_portable(own, opponent, weights);
}

int32_t PatternSet::sum_weights_portable(uint64_t own, uint64_t opponent, std::span<const int16_t> weights) const noexcept {
    assert(weights.size() == raw_size_);
    return direct_.sum_portable(own, opponent, weights);
}

int32_t PatternSet::sum_weights_pext(uint64_t own, uint64_t opponent, std::span<const int16_t> weights) const noexcept {
    assert(weights.size() == raw_size_);
    return direct_.sum_pext(own, opponent, weights.data());
}

} // namespace stello::eval
