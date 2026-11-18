#pragma once

#include "core/bits.hpp"
#include "eval/patterns.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace stello::eval {

inline constexpr size_t MOBILITY_SLOTS_PER_SIDE = 48;
inline constexpr size_t MOBILITY_WEIGHTS_PER_STAGE = 2 * MOBILITY_SLOTS_PER_SIDE;

enum class EvalErrorKind : uint8_t {
    None = 0,
    Io,
    Gzip,
    NotAnEvaluationFile,
    UnknownPatternSet,
    CutShort,
    DoesNotFit,
};

struct EvalError {
    EvalErrorKind kind{EvalErrorKind::None};
    std::string detail;

    bool ok() const noexcept { return kind == EvalErrorKind::None; }
    std::string message() const;
};

class EvalWeights {
public:
    static std::optional<EvalWeights> create(
        const PatternSet* set,
        std::span<const uint8_t> stage_of_discs,
        int32_t units_per_disc,
        std::span<const std::vector<int16_t>> stage_tables,
        EvalError* error = nullptr,
        std::span<const std::vector<int16_t>> mobility_tables = {}
    );

    static const EvalWeights& built_in();
    static std::span<const uint8_t> built_in_file() noexcept;

    static std::optional<EvalWeights> load(const std::filesystem::path& path, EvalError* error = nullptr);
    static std::optional<EvalWeights> read(std::span<const uint8_t> compressed, EvalError* error = nullptr);
    static std::array<uint8_t, 65> even_stages(int32_t stages);

    const PatternSet& set() const noexcept { return *set_; }
    size_t stages() const noexcept { return stages_; }
    int32_t units_per_disc() const noexcept { return units_per_disc_; }
    bool has_mobility() const noexcept { return !mob_weights_.empty(); }

    size_t stage_of(uint32_t discs) const noexcept {
        return static_cast<size_t>(stage_of_discs_[discs]);
    }

    std::span<const int16_t> stage_weights(size_t stage) const noexcept {
        const size_t size = set_->raw_size();
        return {weights_.data() + stage * size, size};
    }

    std::span<const int16_t> stage_mobility_weights(size_t stage) const noexcept {
        if (mob_weights_.empty()) {
            return {};
        }
        return {mob_weights_.data() + stage * MOBILITY_WEIGHTS_PER_STAGE, MOBILITY_WEIGHTS_PER_STAGE};
    }

    int32_t mobility_bonus(size_t stage, uint32_t own_mobility, uint32_t opponent_mobility) const noexcept {
        if (mob_weights_.empty()) {
            return 0;
        }
        const int16_t* const m = mob_weights_.data() + stage * MOBILITY_WEIGHTS_PER_STAGE;
        const uint32_t om = (std::min)(own_mobility, static_cast<uint32_t>(MOBILITY_SLOTS_PER_SIDE - 1U));
        const uint32_t pm = (std::min)(opponent_mobility, static_cast<uint32_t>(MOBILITY_SLOTS_PER_SIDE - 1U));
        return static_cast<int32_t>(m[om]) + static_cast<int32_t>(m[MOBILITY_SLOTS_PER_SIDE + pm]);
    }

    int32_t evaluate(uint64_t own, uint64_t opponent) const noexcept {
        const size_t stage = stage_of(static_cast<uint32_t>(std::popcount(own | opponent)));
        int32_t sum = set_->sum_weights(own, opponent, stage_weights(stage));
        if (!mob_weights_.empty()) {
            const uint32_t om = static_cast<uint32_t>(std::popcount(stello::bits::legal_moves(own, opponent)));
            const uint32_t pm = static_cast<uint32_t>(std::popcount(stello::bits::legal_moves(opponent, own)));
            sum += mobility_bonus(stage, om, pm);
        }
        return sum;
    }

private:
    const PatternSet* set_{nullptr};
    std::array<uint8_t, 65> stage_of_discs_{};
    int32_t units_per_disc_{0};
    size_t stages_{0};
    std::vector<int16_t> weights_;
    std::vector<int16_t> mob_weights_;
};

} // namespace stello::eval
