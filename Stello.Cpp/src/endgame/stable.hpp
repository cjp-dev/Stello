#pragma once

#include <cstdint>

namespace stello::endgame::stable {

/// Builds the edge stability tables eagerly rather than during the first search.
void init();

/// The stable discs of `own` (Egaroucid `calc_stability`). C#: `StableDiscs.Of`.
uint64_t of(uint64_t own, uint64_t opponent);

} // namespace stello::endgame::stable
