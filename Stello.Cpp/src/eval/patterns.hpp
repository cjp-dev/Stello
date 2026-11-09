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
    STELLO_NOINLINE int32_t sum_pext(uint64_t own, uint64_t opponent, const int16_t* weights) const noexcept;

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

    static const PatternSet& classic11();
    static const PatternSet& plus15();
    static const PatternSet& egaroucid16();
    static const PatternSet* by_name(std::string_view name) noexcept;

    void indices(uint64_t own, uint64_t opponent, std::span<uint32_t> out) const noexcept;
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
};

} // namespace stello::eval
