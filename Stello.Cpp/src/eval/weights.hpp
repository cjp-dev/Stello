#pragma once

#include "eval/patterns.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace stello::eval {

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
        EvalError* error = nullptr
    );

    static const EvalWeights& built_in();
    static std::span<const uint8_t> built_in_file() noexcept;

    static std::optional<EvalWeights> load(const std::filesystem::path& path, EvalError* error = nullptr);
    static std::optional<EvalWeights> read(std::span<const uint8_t> compressed, EvalError* error = nullptr);
    static std::array<uint8_t, 65> even_stages(int32_t stages);

    const PatternSet& set() const noexcept { return *set_; }
    size_t stages() const noexcept { return stages_; }
    int32_t units_per_disc() const noexcept { return units_per_disc_; }

    size_t stage_of(uint32_t discs) const noexcept {
        return static_cast<size_t>(stage_of_discs_[discs]);
    }

    std::span<const int16_t> stage_weights(size_t stage) const noexcept {
        const size_t size = set_->raw_size();
        return {weights_.data() + stage * size, size};
    }

    int32_t evaluate(uint64_t own, uint64_t opponent) const noexcept {
        const size_t stage = stage_of(static_cast<uint32_t>(std::popcount(own | opponent)));
        return set_->sum_weights(own, opponent, stage_weights(stage));
    }

private:
    const PatternSet* set_{nullptr};
    std::array<uint8_t, 65> stage_of_discs_{};
    int32_t units_per_disc_{0};
    size_t stages_{0};
    std::vector<int16_t> weights_;
};

} // namespace stello::eval
