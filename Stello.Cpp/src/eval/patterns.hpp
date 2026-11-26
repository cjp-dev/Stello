#pragma once

#include "core/compiler.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace stello::eval {

class PatternShape {
public:
    struct Part {
        uint64_t mask{0};
        uint64_t multiplier{0};
        uint32_t shift{0};
        std::array<uint16_t, 256> digits{};

        size_t extract(uint64_t discs) const noexcept {
            return static_cast<size_t>((((discs & mask) * multiplier) >> shift) & 0xFFULL);
        }

        uint32_t index(uint64_t own, uint64_t opponent) const noexcept {
            return static_cast<uint32_t>(digits[extract(own)])
                 + 2U * static_cast<uint32_t>(digits[extract(opponent)]);
        }
    };

    PatternShape(std::string name, std::span<const uint32_t> squares);

    const std::string& name() const noexcept { return name_; }
    std::span<const uint32_t> squares() const noexcept { return squares_; }
    size_t size() const noexcept { return size_; }
    std::span<const Part> parts() const noexcept { return parts_; }
    std::span<const size_t> placements() const noexcept { return placements_; }
    std::span<const uint32_t> fold() const noexcept { return fold_; }
    std::span<const uint32_t> representatives() const noexcept { return representatives_; }
    std::span<const uint32_t> swap12() const noexcept { return swap12_; }

    uint32_t index(uint64_t own, uint64_t opponent) const noexcept;
    uint32_t slow_index(uint64_t own, uint64_t opponent) const noexcept;

private:
    std::string name_;
    std::vector<uint32_t> squares_;
    size_t size_{0};
    std::vector<Part> parts_;
    std::vector<size_t> placements_;
    std::vector<uint32_t> fold_;
    std::vector<uint32_t> representatives_;
    std::vector<uint32_t> swap12_;
};

namespace detail {

struct Feature {
    size_t shape{0};
    size_t symmetry{0};
    uint32_t offset{0};
    size_t part_begin{0};
    size_t part_end{0};
};

struct DirectPart {
    uint64_t mask{0};
    uint64_t multiplier{0};
    uint32_t shift{0};
    uint32_t table{0};
};

struct PartGroup {
    size_t count{0};
    std::vector<uint32_t> offsets;
    std::vector<DirectPart> parts;
};

struct PextFeature {
    uint64_t mask{0};
    uint32_t table{0};
    uint32_t offset{0};
};

class Direct {
public:
    static Direct build(std::span<const PatternShape> shapes, std::span<const Feature> features);

    STELLO_NOINLINE int32_t sum_portable(uint64_t own, uint64_t opponent, std::span<const int16_t> weights) const noexcept;
    STELLO_NOINLINE int32_t sum_portable(uint64_t own, uint64_t opponent, const int16_t* weights) const noexcept;
    STELLO_NOINLINE int32_t sum_pext(uint64_t own, uint64_t opponent, const int16_t* weights) const noexcept;
    STELLO_NOINLINE int32_t sum_portable_pair(
        uint64_t own,
        uint64_t opponent,
        const int32_t* pair_weights,
        bool odd_slot
    ) const noexcept;
    STELLO_NOINLINE int32_t sum_pext_pair(
        uint64_t own,
        uint64_t opponent,
        const int32_t* pair_weights,
        bool odd_slot
    ) const noexcept;

private:
    template <size_t N>
    int32_t sum_group(const PartGroup& group, uint64_t own, uint64_t opponent, const int16_t* weights) const noexcept;
    int32_t sum_any(const PartGroup& group, uint64_t own, uint64_t opponent, const int16_t* weights) const noexcept;
    uint32_t part_digits(const DirectPart& part, uint64_t own, uint64_t opponent) const noexcept;

    std::vector<PartGroup> groups_;
    std::vector<uint16_t> digits_;
    std::vector<PextFeature> pext_;
    std::vector<uint16_t> pext_tables_;
};

} // namespace detail

inline constexpr size_t MAX_PATTERN_FEATURES = 64;

struct SquareFeatureDelta {
    uint16_t feature{0};
    uint16_t delta{0};
};

struct EvalBucket {
    uint8_t begin{0};
    uint8_t count{0};
    uint32_t base_offset{0};
};

struct alignas(32) PatternState {
    std::array<uint16_t, MAX_PATTERN_FEATURES> features{};
};

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
class PatternSet {
public:
    PatternSet(std::string name, std::vector<PatternShape> shapes);

    const std::string& name() const noexcept { return name_; }
    std::span<const PatternShape> shapes() const noexcept { return shapes_; }
    std::span<const size_t> offsets() const noexcept { return offsets_; }
    size_t raw_size() const noexcept { return raw_size_; }
    size_t feature_count() const noexcept { return features_.size(); }
    const PatternShape& shape_of(size_t feature) const noexcept { return shapes_[features_[feature].shape]; }
    size_t symmetry_of(size_t feature) const noexcept { return features_[feature].symmetry; }

    std::span<const EvalBucket> eval_buckets() const noexcept { return eval_buckets_; }
    const std::vector<size_t>& pack_shape_order() const noexcept { return pack_shape_order_; }
    size_t pack_shape_offset(size_t shape_index) const noexcept { return pack_shape_offsets_[shape_index]; }
    const PatternState& initial_state() const noexcept { return initial_state_; }
    const PatternState& all_1_state() const noexcept { return all_1_state_; }
    const PatternState& all_2_state() const noexcept { return all_2_state_; }
    const uint32_t* slot_offsets() const noexcept { return slot_feature_offsets_.data(); }
    const uint16_t* square_deltas(uint32_t square) const noexcept { return slot_deltas_[square].data(); }
    std::span<const SquareFeatureDelta> square_sparse_deltas(uint32_t square) const noexcept {
        const size_t begin = slot_sparse_offsets_[square];
        const size_t end = slot_sparse_offsets_[square + 1U];
        return {slot_sparse_deltas_.data() + begin, end - begin};
    }

    static const PatternSet& classic11();
    static const PatternSet& plus15();
    static const PatternSet& egaroucid16();
    static const PatternSet* by_name(std::string_view name) noexcept;

    void indices(uint64_t own, uint64_t opponent, std::span<uint32_t> out) const noexcept;
    PatternState state_of(uint64_t digit1, uint64_t digit2) const noexcept;
    PatternState state_of_node(uint64_t own, uint64_t opponent, uint32_t discs) const noexcept {
        return (discs & 1U) == 0U ? state_of(own, opponent) : state_of(opponent, own);
    }
    void advance_state(
        PatternState& next,
        const PatternState& cur,
        bool mover_is_even_disc,
        uint32_t square,
        uint64_t flips
    ) const noexcept;
    int32_t sum_weights_from_state(
        const PatternState& state,
        const int32_t* packed_pair_weights,
        bool odd_slot
    ) const noexcept;
    int32_t sum_weights_after_move(
        const PatternState& cur,
        bool mover_is_even_disc,
        uint32_t square,
        uint64_t flips,
        const int32_t* packed_child_pair_weights,
        bool child_odd_slot
    ) const noexcept;

    int32_t sum_weights_packed(
        uint64_t digit1,
        uint64_t digit2,
        const int32_t* packed_pair_weights,
        bool odd_slot
    ) const noexcept;
    int32_t sum_weights(uint64_t own, uint64_t opponent, std::span<const int16_t> weights) const noexcept;
    int32_t sum_weights_portable(uint64_t own, uint64_t opponent, std::span<const int16_t> weights) const noexcept;
    int32_t sum_weights_pext(uint64_t own, uint64_t opponent, std::span<const int16_t> weights) const noexcept;

private:
    std::string name_;
    std::vector<PatternShape> shapes_;
    std::vector<size_t> offsets_;
    size_t raw_size_{0};
    std::vector<detail::Feature> features_;
    std::vector<size_t> symmetries_;
    std::vector<PatternShape::Part> parts_;
    detail::Direct direct_;
    detail::Direct packed_direct_;
    std::vector<size_t> pack_shape_order_;
    std::vector<size_t> pack_shape_offsets_;
    std::vector<EvalBucket> eval_buckets_;
    PatternState initial_state_{};
    PatternState all_1_state_{};
    PatternState all_2_state_{};
    alignas(32) std::array<std::array<uint16_t, MAX_PATTERN_FEATURES>, 64> slot_deltas_{};
    alignas(32) std::array<uint32_t, MAX_PATTERN_FEATURES> slot_feature_offsets_{};
    std::array<uint16_t, 65> slot_sparse_offsets_{};
    std::vector<SquareFeatureDelta> slot_sparse_deltas_;
};
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif

} // namespace stello::eval
