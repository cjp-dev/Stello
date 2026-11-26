#include "eval/weights.hpp"

#include "core/compiler.hpp"
#include "eval/embedded_eval.hpp"
#include "eval/gzip.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>

namespace stello::eval {
namespace {

constexpr std::string_view kMagic = "STELLOEV";
constexpr uint16_t kVersion = 2;
constexpr uint16_t kMobilityVersion = 3;

void set_error(EvalError* error, EvalErrorKind kind, std::string detail = {}) {
    if (error) {
        error->kind = kind;
        error->detail = std::move(detail);
    }
}

class Reader {
public:
    explicit Reader(std::span<const uint8_t> data) noexcept : data_(data) {}

    std::span<const uint8_t> take_up_to(size_t count) noexcept {
        const size_t avail = data_.size() - at_;
        const size_t take = count < avail ? count : avail;
        const auto slice = data_.subspan(at_, take);
        at_ += take;
        return slice;
    }

    bool take(size_t count, std::span<const uint8_t>& out, EvalError* error) {
        out = take_up_to(count);
        if (out.size() == count) {
            return true;
        }
        set_error(error, EvalErrorKind::CutShort);
        return false;
    }

    bool u8(uint8_t& out, EvalError* error) {
        std::span<const uint8_t> s;
        if (!take(1, s, error)) return false;
        out = s[0];
        return true;
    }

    bool u16(uint16_t& out, EvalError* error) {
        std::span<const uint8_t> s;
        if (!take(2, s, error)) return false;
        out = static_cast<uint16_t>(s[0]) | (static_cast<uint16_t>(s[1]) << 8);
        return true;
    }

private:
    std::span<const uint8_t> data_;
    size_t at_{0};
};

} // namespace

std::string EvalError::message() const {
    switch (kind) {
        case EvalErrorKind::None: return {};
        case EvalErrorKind::Io: return detail;
        case EvalErrorKind::Gzip: return "The evaluation file is not valid gzip: " + detail;
        case EvalErrorKind::NotAnEvaluationFile: return "Not a Stello evaluation file (version 2).";
        case EvalErrorKind::UnknownPatternSet: return "Unknown pattern set " + detail + ".";
        case EvalErrorKind::CutShort: return "The evaluation file is cut short.";
        case EvalErrorKind::DoesNotFit: return "The weights do not fit the pattern set.";
    }
    return {};
}

std::optional<EvalWeights> EvalWeights::create(
    const PatternSet* set,
    std::span<const uint8_t> stage_of_discs,
    int32_t units_per_disc,
    std::span<const std::vector<int16_t>> stage_tables,
    EvalError* error,
    std::span<const std::vector<int16_t>> mobility_tables
) {
    if (error) *error = EvalError{};
    if (!set || stage_of_discs.size() != 65 || stage_tables.empty() || units_per_disc < 1 || units_per_disc > 1024) {
        set_error(error, EvalErrorKind::DoesNotFit);
        return std::nullopt;
    }
    for (const auto& tbl : stage_tables) {
        if (tbl.size() != set->raw_size()) {
            set_error(error, EvalErrorKind::DoesNotFit);
            return std::nullopt;
        }
    }
    if (!mobility_tables.empty()) {
        if (mobility_tables.size() != stage_tables.size()) {
            set_error(error, EvalErrorKind::DoesNotFit);
            return std::nullopt;
        }
        for (const auto& mob : mobility_tables) {
            if (mob.size() != MOBILITY_WEIGHTS_PER_STAGE) {
                set_error(error, EvalErrorKind::DoesNotFit);
                return std::nullopt;
            }
        }
    }
    for (uint8_t s : stage_of_discs) {
        if (static_cast<size_t>(s) >= stage_tables.size()) {
            set_error(error, EvalErrorKind::DoesNotFit);
            return std::nullopt;
        }
    }

    EvalWeights weights;
    weights.set_ = set;
    std::copy_n(stage_of_discs.data(), 65, weights.stage_of_discs_.data());
    weights.units_per_disc_ = units_per_disc;
    weights.stages_ = stage_tables.size();

    constexpr uint32_t kNoSlot = (std::numeric_limits<uint32_t>::max)();
    weights.stage_even_slot_.assign(weights.stages_, kNoSlot);
    weights.stage_odd_slot_.assign(weights.stages_, kNoSlot);
    uint32_t next_even = 0;
    uint32_t next_odd = 0;
    for (uint32_t discs = 0; discs <= 64U; ++discs) {
        const size_t st = static_cast<size_t>(weights.stage_of_discs_[discs]);
        if ((discs & 1U) == 0U) {
            if (weights.stage_even_slot_[st] == kNoSlot) {
                weights.stage_even_slot_[st] = next_even++;
            }
        } else {
            if (weights.stage_odd_slot_[st] == kNoSlot) {
                weights.stage_odd_slot_[st] = next_odd++;
            }
        }
    }

    const size_t num_pairs = static_cast<size_t>((std::max)(next_even, next_odd));
    const size_t raw_size = set->raw_size();
    weights.weights_.assign(num_pairs * raw_size, 0);
    const auto shapes = set->shapes();
    const auto raw_offsets = set->offsets();
    const auto& shape_order = set->pack_shape_order();

    for (size_t st = 0; st < weights.stages_; ++st) {
        const int16_t* const src_stage = stage_tables[st].data();
        if (weights.stage_even_slot_[st] != kNoSlot) {
            int32_t* const dst = weights.weights_.data() + static_cast<size_t>(weights.stage_even_slot_[st]) * raw_size;
            for (const size_t s : shape_order) {
                const size_t sz = shapes[s].size();
                const int16_t* const src_shape = src_stage + raw_offsets[s];
                int32_t* const dst_shape = dst + set->pack_shape_offset(s);
                for (size_t r = 0; r < sz; ++r) {
                    const uint32_t lo = static_cast<uint16_t>(src_shape[r]);
                    dst_shape[r] = static_cast<int32_t>((static_cast<uint32_t>(dst_shape[r]) & 0xFFFF0000U) | lo);
                }
            }
        }
        if (weights.stage_odd_slot_[st] != kNoSlot) {
            int32_t* const dst = weights.weights_.data() + static_cast<size_t>(weights.stage_odd_slot_[st]) * raw_size;
            for (const size_t s : shape_order) {
                const size_t sz = shapes[s].size();
                const int16_t* const src_shape = src_stage + raw_offsets[s];
                int32_t* const dst_shape = dst + set->pack_shape_offset(s);
                const auto sw = shapes[s].swap12();
                for (size_t r = 0; r < sz; ++r) {
                    const uint32_t hi = static_cast<uint32_t>(static_cast<uint16_t>(src_shape[r])) << 16;
                    const size_t dr = sw[r];
                    dst_shape[dr] = static_cast<int32_t>((static_cast<uint32_t>(dst_shape[dr]) & 0x0000FFFFU) | hi);
                }
            }
        }
    }

    for (uint32_t discs = 0; discs <= 64U; ++discs) {
        const size_t st = static_cast<size_t>(weights.stage_of_discs_[discs]);
        const uint32_t pair_idx = ((discs & 1U) == 0U) ? weights.stage_even_slot_[st] : weights.stage_odd_slot_[st];
        weights.parity_ptrs_[discs] = weights.weights_.data() + static_cast<size_t>(pair_idx) * raw_size;
    }

    if (!mobility_tables.empty()) {
        weights.mob_weights_.reserve(mobility_tables.size() * MOBILITY_WEIGHTS_PER_STAGE);
        for (size_t st = 0; st < weights.stages_; ++st) {
            const auto& mob = mobility_tables[st];
            weights.mob_weights_.insert(weights.mob_weights_.end(), mob.begin(), mob.end());
        }
    }
    return weights;
}

std::vector<int16_t> EvalWeights::stage_weights_copy(size_t stage) const {
    assert(stage < stages_);
    const size_t raw_size = set_->raw_size();
    std::vector<int16_t> out(raw_size, 0);
    const auto shapes = set_->shapes();
    const auto raw_offsets = set_->offsets();
    constexpr uint32_t kNoSlot = (std::numeric_limits<uint32_t>::max)();
    if (stage_even_slot_[stage] != kNoSlot) {
        const int32_t* const src = weights_.data() + static_cast<size_t>(stage_even_slot_[stage]) * raw_size;
        for (const size_t s : set_->pack_shape_order()) {
            const size_t sz = shapes[s].size();
            const int32_t* const src_shape = src + set_->pack_shape_offset(s);
            int16_t* const dst_shape = out.data() + raw_offsets[s];
            for (size_t r = 0; r < sz; ++r) {
                dst_shape[r] = static_cast<int16_t>(static_cast<uint16_t>(src_shape[r]));
            }
        }
    } else if (stage_odd_slot_[stage] != kNoSlot) {
        const int32_t* const src = weights_.data() + static_cast<size_t>(stage_odd_slot_[stage]) * raw_size;
        for (const size_t s : set_->pack_shape_order()) {
            const size_t sz = shapes[s].size();
            const int32_t* const src_shape = src + set_->pack_shape_offset(s);
            int16_t* const dst_shape = out.data() + raw_offsets[s];
            const auto sw = shapes[s].swap12();
            for (size_t r = 0; r < sz; ++r) {
                dst_shape[r] = static_cast<int16_t>(src_shape[sw[r]] >> 16);
            }
        }
    }
    return out;
}

const EvalWeights& EvalWeights::built_in() {
    static const EvalWeights kBuiltIn = [] {
        EvalError err;
        auto w = read(built_in_file(), &err);
        assert(w.has_value() && "the built-in evaluation must be valid");
        return std::move(*w);
    }();
    return kBuiltIn;
}

std::span<const uint8_t> EvalWeights::built_in_file() noexcept {
    return detail::embedded_eval_bytes();
}

std::optional<EvalWeights> EvalWeights::load(const std::filesystem::path& path, EvalError* error) {
    if (error) *error = EvalError{};
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        set_error(error, EvalErrorKind::Io, "Could not open evaluation file: " + path.string());
        return std::nullopt;
    }
    const auto size = file.tellg();
    if (size < 0) {
        set_error(error, EvalErrorKind::Io, "Could not read evaluation file size: " + path.string());
        return std::nullopt;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (size > 0 && !file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        set_error(error, EvalErrorKind::Io, "Could not read evaluation file: " + path.string());
        return std::nullopt;
    }
    return read(bytes, error);
}

std::optional<EvalWeights> EvalWeights::read(std::span<const uint8_t> compressed, EvalError* error) {
    if (error) *error = EvalError{};
    auto gz = gzip::decompress(compressed);
    if (!gz.ok()) {
        set_error(error, EvalErrorKind::Gzip, std::move(gz.error));
        return std::nullopt;
    }

    Reader reader(gz.data);
    const auto magic = reader.take_up_to(kMagic.size());
    uint16_t version = 0;
    if (magic.size() != kMagic.size() || std::memcmp(magic.data(), kMagic.data(), kMagic.size()) != 0) {
        set_error(error, EvalErrorKind::NotAnEvaluationFile);
        return std::nullopt;
    }
    if (!reader.u16(version, error)) {
        return std::nullopt;
    }
    if (version != kVersion && version != kMobilityVersion) {
        set_error(error, EvalErrorKind::NotAnEvaluationFile);
        return std::nullopt;
    }

    uint8_t name_len = 0;
    if (!reader.u8(name_len, error)) return std::nullopt;
    const auto name_bytes = reader.take_up_to(name_len);
    std::string name(reinterpret_cast<const char*>(name_bytes.data()), name_bytes.size());
    const PatternSet* set = PatternSet::by_name(name);
    if (!set) {
        set_error(error, EvalErrorKind::UnknownPatternSet, std::move(name));
        return std::nullopt;
    }

    uint8_t stages_u8 = 0;
    if (!reader.u8(stages_u8, error)) return std::nullopt;
    const size_t stages = stages_u8;

    const auto stage_of_discs = reader.take_up_to(65);
    uint16_t units_u16 = 0;
    if (!reader.u16(units_u16, error)) return std::nullopt;
    const int32_t units_per_disc = static_cast<int32_t>(units_u16);

    size_t count = 0;
    for (const auto& shape : set->shapes()) {
        count += shape.representatives().size();
    }

    std::vector<std::vector<int16_t>> weights;
    weights.reserve(stages);
    std::vector<int16_t> previous(count, 0);

    for (size_t stage = 0; stage < stages; ++stage) {
        std::span<const uint8_t> low;
        std::span<const uint8_t> high;
        if (!reader.take(count, low, error) || !reader.take(count, high, error)) {
            return std::nullopt;
        }
        for (size_t i = 0; i < count; ++i) {
            const uint16_t delta_u = static_cast<uint16_t>(low[i]) | (static_cast<uint16_t>(high[i]) << 8);
            previous[i] = wrapping_add_i16(previous[i], static_cast<int16_t>(delta_u));
        }

        std::vector<int16_t> table(set->raw_size(), 0);
        size_t next = 0;
        const auto shapes = set->shapes();
        const auto offsets = set->offsets();
        for (size_t s = 0; s < shapes.size(); ++s) {
            const auto& shape = shapes[s];
            std::vector<int16_t> folded(shape.size(), 0);
            for (uint32_t rep : shape.representatives()) {
                folded[rep] = previous[next++];
            }
            const size_t offset = offsets[s];
            const auto fold = shape.fold();
            for (size_t raw = 0; raw < fold.size(); ++raw) {
                table[offset + raw] = folded[fold[raw]];
            }
        }
        weights.push_back(std::move(table));
    }

    std::vector<std::vector<int16_t>> mobility_weights;
    if (version == kMobilityVersion) {
        mobility_weights.reserve(stages);
        std::vector<int16_t> mob_prev(MOBILITY_WEIGHTS_PER_STAGE, 0);
        for (size_t stage = 0; stage < stages; ++stage) {
            std::span<const uint8_t> low;
            std::span<const uint8_t> high;
            if (!reader.take(MOBILITY_WEIGHTS_PER_STAGE, low, error)
                || !reader.take(MOBILITY_WEIGHTS_PER_STAGE, high, error)) {
                return std::nullopt;
            }
            for (size_t i = 0; i < MOBILITY_WEIGHTS_PER_STAGE; ++i) {
                const uint16_t delta_u = static_cast<uint16_t>(low[i]) | (static_cast<uint16_t>(high[i]) << 8);
                mob_prev[i] = wrapping_add_i16(mob_prev[i], static_cast<int16_t>(delta_u));
            }
            mobility_weights.push_back(mob_prev);
        }
    }

    return create(set, stage_of_discs, units_per_disc, weights, error, mobility_weights);
}

std::array<uint8_t, 65> EvalWeights::even_stages(int32_t stages) {
    assert(stages >= 1 && stages <= 61);
    std::array<uint8_t, 65> out{};
    for (int32_t discs = 0; discs <= 64; ++discs) {
        const int32_t stage = ((discs - 4) * stages) / 61;
        out[static_cast<size_t>(discs)] = static_cast<uint8_t>(std::clamp(stage, 0, stages - 1));
    }
    return out;
}

} // namespace stello::eval
