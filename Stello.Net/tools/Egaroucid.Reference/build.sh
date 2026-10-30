#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
#
# Builds the C++ Egaroucid reference used to check the C# port (see README.md):
#   bin/egaroucid-generic  Egaroucid for Console, Generic build (no AVX2), as the port is the generic code
#   bin/egaroucid-ref      reference.cpp, which prints evaluations, perft, search results and node counts
# and copies Egaroucid's resources next to them, where both programs look for them.
#
# Usage: build.sh [path to Egaroucid-main]   (default: <repo>/Egaroucid-main)
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
egaroucid="${1:-$here/../../../Egaroucid-main}"
egaroucid="$(cd "$egaroucid" && pwd)"
out="$here/bin"

if [[ ! -f "$egaroucid/src/engine/engine_all.hpp" ]]; then
    echo "Egaroucid source not found in $egaroucid" >&2
    exit 1
fi

# -DHAS_NO_AVX2: the generic code paths, which the C# port follows (the SIMD build counts endgame nodes differently).
# -ffp-contract=off: no fused multiply-add, so the floating-point Multi-ProbCut table matches .NET's arithmetic.
flags=(-O2 -std=c++20 -pthread -DHAS_NO_AVX2 -ffp-contract=off -w)

mkdir -p "$out/resources"
cp -r "$egaroucid/bin/resources/eval.egev2" "$egaroucid/bin/resources/eval_move_ordering_end.egev" \
      "$egaroucid/bin/resources/book.egbk3" "$egaroucid/bin/resources/hash" "$out/resources/"

echo "Building egaroucid-generic ..."
g++ "${flags[@]}" "$egaroucid/src/Egaroucid_for_Console.cpp" -o "$out/egaroucid-generic"
echo "Building egaroucid-ref ..."
g++ "${flags[@]}" -I"$egaroucid/src" "$here/reference.cpp" -o "$out/egaroucid-ref"
echo "Done: $out"
