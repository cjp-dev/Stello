#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace stello::search {

struct MicrobenchOptions {
    uint64_t warmup{1000};
    uint64_t repeats{500000};
    std::string_view only;
    bool verbose{false};
};

struct MicrobenchStats {
    std::string_view name;
    double min_cycles{0.0};
    double mean_cycles{0.0};
    double stddev_cycles{0.0};
    double max_cycles{0.0};
    uint64_t checksum{0};
    std::vector<double> per_sample;
};

std::vector<std::string_view> microbench_names();

std::vector<MicrobenchStats> run_microbench(const MicrobenchOptions& opts, bool use_avx2);

} // namespace stello::search
