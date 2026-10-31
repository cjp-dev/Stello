// SPDX-License-Identifier: GPL-3.0-only
// The search of the last 13 empty squares is adapted from Egaroucid src/engine/endsearch_nws.hpp,
// endsearch_nws_last_generic.hpp and stability_cutoff.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana,
// GPL-3.0-or-later, through the C# port in Stello.Net/Egaroucid.Engine/EndSearch.cs. Above 13 empty squares the
// null-window and principal-variation search follows the shape of Egaroucid's midsearch_nws.hpp and midsearch.hpp
// (transposition cutoffs, enhanced transposition cutoff, move ordering by mobility and a shallow search), written on
// Stello's transposition table and ordered by Stello's pattern evaluation.

using System.Numerics;
using System.Runtime.CompilerServices;
using Stello.Engine.Endgame;
using Stello.Engine.Search;
using static Stello.Engine.Endgame.EndgameBits;

namespace Stello.Engine;

/// <summary>
/// Stello 2's exact endgame solver. Scores are final disc differences for the side to move, the empty squares going
/// to the winner. Null-window searches return fail-soft bounds: a score above alpha is a lower bound, at or below
/// alpha an upper bound.
/// </summary>
public sealed partial class SearchEngine
{
    private const int EndInfinity = 127;
    private const int EndFastEmpties = 6;
    private const int EndSimpleEmpties = 10;
    private const int EndLocalEmpties = 13;
    private const int EndEtcMinEmpties = 12;

    // In solve mode, the midgame search to this depth first orders the moves at the root.
    private const int SolveOrderingLooks = 4;

    // Egaroucid's move-ordering weights (move_ordering.hpp); values are compared against an offset of 38.
    private const int OrderOffset = 38;
    private const int OrderMobility = 35;
    private const int OrderPotentialMobility = 17;
    private const int OrderValue = 269;
    private const int OrderValueDeep = 94;
    private const int OrderNullMobility = 17;
    private const int OrderNullValue = 7;
    private const int OrderNullValueDeep = 25;
    private const int OrderLocalMobility = 40;
    private const int OrderSimpleMobility = 18;
    private const int OrderSimpleParity = 17;
    private const int OrderHashMove = 10_000_000;

    private readonly LocalTable _localTable = new();
    private long _nextAbortCheck;

    private struct EndMove
    {
        public int Square;
        public ulong Flips;
        public int Value;

        /// <summary>The opponent's moves after this move, or <see cref="LegalUndefined"/>.</summary>
        public ulong Replies;
    }

    private int SolveChild(ulong own, ulong opponent, int alpha, int beta, int empties)
    {
        if (!_newEndgame)
        {
            return Solve(own, opponent, alpha, beta, empties);
        }

        return beta - alpha == 1
            ? EndNull(own, opponent, alpha, empties, LegalUndefined)
            : EndWindow(own, opponent, alpha, beta, empties, passed: false);
    }

    // A null-window search of a position reached by a move.
    private int EndNull(ulong own, ulong opponent, int alpha, int empties, ulong legal)
    {
        if (empties <= 4)
        {
            return SolveShallow(own, opponent, alpha, alpha + 1, empties, passed: false);
        }

        return empties <= EndLocalEmpties
            ? EndLocal(own, opponent, alpha, passed: false, legal, empties)
            : EndDeep(own, opponent, alpha, empties, passed: false, legal);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private void CountEndNode()
    {
        if (++_nodes >= _nextAbortCheck)
        {
            _nextAbortCheck = _nodes + 1024;
            CheckAbort();
        }
    }

    // ---- With a window (principal variation nodes) ----

    private int EndWindow(ulong own, ulong opponent, int alpha, int beta, int empties, bool passed)
    {
        if (empties <= EndFastEmpties)
        {
            return SolveShallow(own, opponent, alpha, beta, empties, passed);
        }

        CountEndNode();
        ulong legal = LegalMoves(own, opponent);
        if (legal == 0)
        {
            if (passed)
            {
                _evaluations++;
                return FinalScore(own, opponent);
            }

            return -EndWindow(opponent, own, -beta, -alpha, empties, passed: true);
        }

        if (!passed && beta >= StabilityThreshold[64 - empties])
        {
            // The opponent's stable discs cap the score.
            int limit = 64 - 2 * BitOperations.PopCount(StableDiscs.Of(opponent, own));
            if (limit <= alpha)
            {
                return limit;
            }

            beta = Math.Min(beta, limit);
        }

        int hashMove = -1;
        if (_endgameTable.TryGet(own, opponent, 0, out TranspositionTable.Entry entry))
        {
            if (entry.Bound == Bound.Exact)
            {
                return entry.Value;
            }

            if (entry.Bound == Bound.Lower)
            {
                alpha = Math.Max(alpha, entry.Value);
            }
            else
            {
                beta = Math.Min(beta, entry.Value);
            }

            if (alpha >= beta)
            {
                return entry.Value;
            }

            hashMove = entry.Move;
        }

        int count = BitOperations.PopCount(legal);
        Span<EndMove> moves = stackalloc EndMove[count];
        if (FillEndMoves(own, opponent, legal, moves))
        {
            return 64;
        }

        if (empties >= EndEtcMinEmpties)
        {
            // A child whose stored upper bound already gives a cutoff makes the search unnecessary.
            foreach (ref EndMove move in moves)
            {
                (ulong childOwn, ulong childOpponent) = Child(own, opponent, move);
                if (_endgameTable.TryGet(childOwn, childOpponent, 0, out TranspositionTable.Entry child)
                    && child.Bound != Bound.Lower && -child.Value >= beta)
                {
                    return -child.Value;
                }
            }
        }

        if (count > 1)
        {
            int depth = Math.Min(empties >> 3, 2);
            int orderAlpha = -Math.Min(64, beta + 8);
            int orderBeta = -Math.Max(-64, alpha - 12);
            ulong empty = ~(own | opponent);
            foreach (ref EndMove move in moves)
            {
                if (move.Square == hashMove)
                {
                    move.Value = OrderHashMove;
                    continue;
                }

                (ulong childOwn, ulong childOpponent) = Child(own, opponent, move);
                move.Replies = LegalMoves(childOwn, childOpponent);
                ulong childEmpty = empty & ~(1UL << move.Square);
                move.Value = (OrderOffset - WeightedMoves(move.Replies)) * OrderMobility
                    + (OrderOffset - PotentialMobility(childOpponent, childEmpty)) * OrderPotentialMobility
                    + (64 - OrderingValue(childOwn, childOpponent, depth, orderAlpha, orderBeta)) * (OrderValue + depth * OrderValueDeep);
            }
        }

        int alphaStart = alpha;
        int best = -EndInfinity;
        int bestMove = -1;
        for (int i = 0; i < count; i++)
        {
            SwapNextBest(moves, i);
            ref EndMove move = ref moves[i];
            (ulong childOwn, ulong childOpponent) = Child(own, opponent, move);
            int score;
            if (i == 0)
            {
                score = -EndWindow(childOwn, childOpponent, -beta, -alpha, empties - 1, passed: false);
            }
            else
            {
                score = -EndNull(childOwn, childOpponent, -alpha - 1, empties - 1, move.Replies);
                if (score > alpha && score < beta)
                {
                    score = -EndWindow(childOwn, childOpponent, -beta, -score, empties - 1, passed: false);
                }
            }

            if (score > best)
            {
                best = score;
                bestMove = move.Square;
                if (best > alpha)
                {
                    alpha = best;
                    if (alpha >= beta)
                    {
                        break;
                    }
                }
            }
        }

        Bound bound = best >= beta ? Bound.Lower : best > alphaStart ? Bound.Exact : Bound.Upper;
        _endgameTable.Store(own, opponent, 0, empties, bound, best, bestMove);
        return best;
    }

    // ---- Null window above 13 empty squares ----

    private int EndDeep(ulong own, ulong opponent, int alpha, int empties, bool passed, ulong legal)
    {
        if (empties <= EndLocalEmpties)
        {
            return EndLocal(own, opponent, alpha, passed, legal, empties);
        }

        CountEndNode();
        if (legal == LegalUndefined)
        {
            legal = LegalMoves(own, opponent);
        }

        if (legal == 0)
        {
            if (passed)
            {
                _evaluations++;
                return FinalScore(own, opponent);
            }

            return -EndDeep(opponent, own, -alpha - 1, empties, passed: true, LegalUndefined);
        }

        if (!passed)
        {
            int cut = StabilityCut(own, opponent, alpha, empties);
            if (cut != int.MinValue)
            {
                return cut;
            }
        }

        int hashMove = -1;
        if (_endgameTable.TryGet(own, opponent, 0, out TranspositionTable.Entry entry))
        {
            if (entry.Bound != Bound.Upper && entry.Value > alpha)
            {
                return entry.Value;
            }

            if (entry.Bound != Bound.Lower && entry.Value <= alpha)
            {
                return entry.Value;
            }

            hashMove = entry.Move;
        }

        int count = BitOperations.PopCount(legal);
        Span<EndMove> moves = stackalloc EndMove[count];
        if (FillEndMoves(own, opponent, legal, moves))
        {
            return 64;
        }

        int best = -EndInfinity;
        int bestMove = -1;
        ulong done = 0;
        if (empties >= EndEtcMinEmpties)
        {
            // Enhanced transposition cutoff: the children's stored bounds may already decide this node.
            foreach (ref EndMove move in moves)
            {
                (ulong childOwn, ulong childOpponent) = Child(own, opponent, move);
                if (!_endgameTable.TryGet(childOwn, childOpponent, 0, out TranspositionTable.Entry child))
                {
                    continue;
                }

                if (child.Bound != Bound.Lower && -child.Value > alpha)
                {
                    return -child.Value;
                }

                if (child.Bound != Bound.Upper && -child.Value <= alpha)
                {
                    best = Math.Max(best, -child.Value);
                    done |= 1UL << move.Square;
                }
            }
        }

        if (hashMove >= 0 && ((legal & ~done) & (1UL << hashMove)) != 0)
        {
            // The stored best move first, before spending time on ordering the others.
            foreach (ref EndMove move in moves)
            {
                if (move.Square == hashMove)
                {
                    (ulong childOwn, ulong childOpponent) = Child(own, opponent, move);
                    int score = -EndDeep(childOwn, childOpponent, -alpha - 1, empties - 1, passed: false, LegalUndefined);
                    if (score > best)
                    {
                        best = score;
                        bestMove = hashMove;
                    }

                    done |= 1UL << hashMove;
                    break;
                }
            }
        }

        if (best <= alpha)
        {
            if (count > 1)
            {
                int depth = empties >> 4;
                int orderAlpha = -Math.Min(64, alpha + 6);
                int orderBeta = -Math.Max(-64, alpha - 16);
                foreach (ref EndMove move in moves)
                {
                    if ((done & (1UL << move.Square)) != 0)
                    {
                        move.Value = int.MinValue;
                        continue;
                    }

                    (ulong childOwn, ulong childOpponent) = Child(own, opponent, move);
                    move.Replies = LegalMoves(childOwn, childOpponent);
                    move.Value = (OrderOffset - WeightedMoves(move.Replies)) * OrderNullMobility
                        + (64 - OrderingValue(childOwn, childOpponent, depth, orderAlpha, orderBeta)) * (OrderNullValue + depth * OrderNullValueDeep);
                }
            }

            for (int i = 0; i < count; i++)
            {
                SwapNextBest(moves, i);
                ref EndMove move = ref moves[i];
                if ((done & (1UL << move.Square)) != 0)
                {
                    continue;
                }

                (ulong childOwn, ulong childOpponent) = Child(own, opponent, move);
                int score = -EndDeep(childOwn, childOpponent, -alpha - 1, empties - 1, passed: false, move.Replies);
                if (score > best)
                {
                    best = score;
                    bestMove = move.Square;
                    if (best > alpha)
                    {
                        break;
                    }
                }
            }
        }

        _endgameTable.Store(own, opponent, 0, empties, best > alpha ? Bound.Lower : Bound.Upper, best, bestMove);
        return best;
    }

    // The value of a position for ordering, in discs: the pattern evaluation after a small alpha-beta search.
    private int OrderingValue(ulong own, ulong opponent, int depth, int alpha, int beta)
    {
        if (depth == 0)
        {
            _evaluations++;
            return _pattern is null ? 0 : Math.Clamp(_pattern.Evaluate(own, opponent) / _pattern.UnitsPerDisc, -64, 64);
        }

        _nodes++;
        ulong legal = LegalMoves(own, opponent);
        if (legal == 0)
        {
            return LegalMoves(opponent, own) == 0
                ? FinalScore(own, opponent)
                : -OrderingValue(opponent, own, depth, -beta, -alpha);
        }

        int best = -EndInfinity;
        for (; legal != 0; legal &= legal - 1)
        {
            int square = BitOperations.TrailingZeroCount(legal);
            ulong flips = Flips(own, opponent, square);
            int score = -OrderingValue(opponent ^ flips, own ^ flips ^ (1UL << square), depth - 1, -beta, -alpha);
            if (score > best)
            {
                best = score;
                if (best > alpha)
                {
                    alpha = best;
                    if (alpha >= beta)
                    {
                        break;
                    }
                }
            }
        }

        return best;
    }

    // ---- Null window at 11 to 13 empty squares, with the local table ----

    private int EndLocal(ulong own, ulong opponent, int alpha, bool passed, ulong legal, int empties)
    {
        if (empties <= EndSimpleEmpties)
        {
            return EndSimple(own, opponent, alpha, passed, legal, empties);
        }

        CountEndNode();
        if (!passed)
        {
            int cut = StabilityCut(own, opponent, alpha, empties);
            if (cut != int.MinValue)
            {
                return cut;
            }
        }

        if (legal == LegalUndefined)
        {
            legal = LegalMoves(own, opponent);
        }

        if (legal == 0)
        {
            return passed ? FinalScore(own, opponent) : -EndLocal(opponent, own, -alpha - 1, passed: true, LegalUndefined, empties);
        }

        int count = BitOperations.PopCount(legal);
        Span<EndMove> moves = stackalloc EndMove[count];
        if (FillEndMoves(own, opponent, legal, moves))
        {
            return 64;
        }

        int best = -EndInfinity;
        ulong done = 0;
        if (count > 1)
        {
            foreach (ref EndMove move in moves)
            {
                (ulong childOwn, ulong childOpponent) = Child(own, opponent, move);
                ref LocalTable.Entry stored = ref _localTable.Get(childOwn, childOpponent, empties - 1);
                if (stored.Matches(childOwn, childOpponent))
                {
                    if (alpha < stored.Lower)
                    {
                        return stored.Lower;
                    }

                    if (stored.Upper <= alpha)
                    {
                        best = Math.Max(best, stored.Upper);
                        done |= 1UL << move.Square;
                        continue;
                    }
                }

                move.Replies = LegalMoves(childOwn, childOpponent);
                int replies = MovesCornersTwice(move.Replies);
                if (replies <= 1)
                {
                    // At most one reply: search it now, it is cheap and often decides the node.
                    int score = -EndLocal(childOwn, childOpponent, -alpha - 1, passed: false, move.Replies, empties - 1);
                    if (score > best)
                    {
                        best = score;
                        if (alpha < best)
                        {
                            stored.Set(childOwn, childOpponent, best, 64);
                            return best;
                        }
                    }

                    stored.Set(childOwn, childOpponent, -64, score);
                    done |= 1UL << move.Square;
                    continue;
                }

                // Egaroucid adds a small move-ordering evaluation here. The pattern evaluation saves about 20 % of
                // the nodes but costs more time than that, so the opponent's mobility orders alone.
                move.Value = (OrderOffset - replies) * OrderLocalMobility;
            }
        }

        for (int i = 0; i < count; i++)
        {
            SwapNextBest(moves, i);
            ref EndMove move = ref moves[i];
            if ((done & (1UL << move.Square)) != 0)
            {
                continue;
            }

            (ulong childOwn, ulong childOpponent) = Child(own, opponent, move);
            ref LocalTable.Entry stored = ref _localTable.Get(childOwn, childOpponent, empties - 1);
            int score = -EndLocal(childOwn, childOpponent, -alpha - 1, passed: false, move.Replies, empties - 1);
            if (score > best)
            {
                best = score;
                if (alpha < best)
                {
                    stored.Set(childOwn, childOpponent, best, 64);
                    break;
                }
            }

            stored.Set(childOwn, childOpponent, -64, score);
        }

        return best;
    }

    // ---- Null window at 7 to 10 empty squares: parity and mobility ordering ----

    private int EndSimple(ulong own, ulong opponent, int alpha, bool passed, ulong legal, int empties)
    {
        if (empties <= EndFastEmpties)
        {
            return EndFast(own, opponent, alpha, passed, empties);
        }

        CountEndNode();
        if (!passed)
        {
            int cut = StabilityCut(own, opponent, alpha, empties);
            if (cut != int.MinValue)
            {
                return cut;
            }
        }

        if (legal == LegalUndefined)
        {
            legal = LegalMoves(own, opponent);
        }

        if (legal == 0)
        {
            return passed ? FinalScore(own, opponent) : -EndSimple(opponent, own, -alpha - 1, passed: true, LegalUndefined, empties);
        }

        int count = BitOperations.PopCount(legal);
        Span<EndMove> moves = stackalloc EndMove[count];
        if (FillEndMoves(own, opponent, legal, moves))
        {
            return 64;
        }

        int best = -EndInfinity;
        ulong done = 0;
        if (count > 1)
        {
            int parity = Parity(~(own | opponent));
            foreach (ref EndMove move in moves)
            {
                move.Value = (parity & QuadrantBit[move.Square]) != 0 ? OrderSimpleParity : 0;
                (ulong childOwn, ulong childOpponent) = Child(own, opponent, move);
                ref LocalTable.Entry stored = ref _localTable.Get(childOwn, childOpponent, empties - 1);
                if (stored.Matches(childOwn, childOpponent))
                {
                    if (alpha < stored.Lower)
                    {
                        return stored.Lower;
                    }

                    if (stored.Upper <= alpha)
                    {
                        best = Math.Max(best, stored.Upper);
                        done |= 1UL << move.Square;
                        continue;
                    }
                }

                move.Replies = LegalMoves(childOwn, childOpponent);
                int replies = MovesCornersTwice(move.Replies);
                if (replies <= 1)
                {
                    int score = -EndSimple(childOwn, childOpponent, -alpha - 1, passed: false, move.Replies, empties - 1);
                    if (score > best)
                    {
                        best = score;
                        if (alpha < best)
                        {
                            stored.Set(childOwn, childOpponent, best, 64);
                            return best;
                        }
                    }

                    stored.Set(childOwn, childOpponent, -64, score);
                    done |= 1UL << move.Square;
                    continue;
                }

                move.Value += (OrderOffset - replies) * OrderSimpleMobility;
            }
        }

        for (int i = 0; i < count; i++)
        {
            SwapNextBest(moves, i);
            ref EndMove move = ref moves[i];
            if ((done & (1UL << move.Square)) != 0)
            {
                continue;
            }

            (ulong childOwn, ulong childOpponent) = Child(own, opponent, move);
            ref LocalTable.Entry stored = ref _localTable.Get(childOwn, childOpponent, empties - 1);
            int score = -EndSimple(childOwn, childOpponent, -alpha - 1, passed: false, move.Replies, empties - 1);
            if (score > best)
            {
                best = score;
                if (alpha < best)
                {
                    stored.Set(childOwn, childOpponent, best, 64);
                    break;
                }
            }

            stored.Set(childOwn, childOpponent, -64, score);
        }

        return best;
    }

    // ---- Null window at 5 and 6 empty squares: lone empty squares first ----

    private int EndFast(ulong own, ulong opponent, int alpha, bool passed, int empties)
    {
        CountEndNode();
        if (!passed)
        {
            int cut = StabilityCut(own, opponent, alpha, empties);
            if (cut != int.MinValue)
            {
                return cut;
            }
        }

        ulong legal = LegalMoves(own, opponent);
        if (legal == 0)
        {
            return passed ? FinalScore(own, opponent) : -EndFast(opponent, own, -alpha - 1, passed: true, empties);
        }

        int best = -EndInfinity;
        ulong first = legal & LoneEmpties(own, opponent);
        if (first == 0)
        {
            first = legal;
        }

        for (ulong batch = first; batch != 0; batch = legal)
        {
            legal ^= batch;
            for (; batch != 0; batch &= batch - 1)
            {
                int square = BitOperations.TrailingZeroCount(batch);
                ulong flips = Flips(own, opponent, square);
                int score = empties == 5
                    ? Last4(opponent ^ flips, own ^ flips ^ (1UL << square), alpha)
                    : -EndFast(opponent ^ flips, own ^ flips ^ (1UL << square), -alpha - 1, passed: false, empties - 1);
                if (score > alpha)
                {
                    return score;
                }

                best = Math.Max(best, score);
            }
        }

        return best;
    }

    // ---- The last 4 empty squares (Egaroucid last4_nws ... last1) ----
    // Last4 and Last2 return the score for the side that moved into their position; Last3 and Last1 for the side to
    // move. The pass at each level flips the sign through pol.

    private int Last4(ulong own, ulong opponent, int alpha)
    {
        ulong empties = ~(own | opponent);
        ulong lone = LoneEmpties(own, opponent);
        if (lone == 0)
        {
            lone = empties;
        }

        ulong rest = empties & ~lone;
        int p0 = BitOperations.TrailingZeroCount(lone);
        if ((lone &= lone - 1) == 0)
        {
            lone = rest;
        }

        int p1 = BitOperations.TrailingZeroCount(lone);
        if ((lone &= lone - 1) == 0)
        {
            lone = rest;
        }

        int p2 = BitOperations.TrailingZeroCount(lone);
        int p3 = (lone &= lone - 1) != 0 ? BitOperations.TrailingZeroCount(lone) : BitOperations.TrailingZeroCount(rest);

        ulong p = own, o = opponent;
        int v = EndInfinity;
        int pol = 1;
        do
        {
            _nodes++;
            ulong flips;
            if ((FlipNeighbours[p0] & o) != 0 && (flips = Flips(p, o, p0)) != 0)
            {
                v = Last3(o ^ flips, p ^ flips ^ (1UL << p0), alpha, p1, p2, p3, empties);
                if (alpha >= v)
                {
                    return v * pol;
                }
            }

            int g;
            if ((FlipNeighbours[p1] & o) != 0 && (flips = Flips(p, o, p1)) != 0)
            {
                g = Last3(o ^ flips, p ^ flips ^ (1UL << p1), alpha, p0, p2, p3, empties);
                if (alpha >= g)
                {
                    return g * pol;
                }

                v = Math.Min(v, g);
            }

            if ((FlipNeighbours[p2] & o) != 0 && (flips = Flips(p, o, p2)) != 0)
            {
                g = Last3(o ^ flips, p ^ flips ^ (1UL << p2), alpha, p0, p1, p3, empties);
                if (alpha >= g)
                {
                    return g * pol;
                }

                v = Math.Min(v, g);
            }

            if ((FlipNeighbours[p3] & o) != 0 && (flips = Flips(p, o, p3)) != 0)
            {
                g = Last3(o ^ flips, p ^ flips ^ (1UL << p3), alpha, p0, p1, p2, empties);
                return Math.Min(v, g) * pol;
            }

            if (v < EndInfinity)
            {
                return v * pol;
            }

            (p, o) = (o, p);
            alpha = -alpha - 1;
        }
        while ((pol = -pol) < 0);

        return -ScoreWithEmpties(own, 4);
    }

    private int Last3(ulong own, ulong opponent, int alpha, int p0, int p1, int p2, ulong empties4)
    {
        // A square alone in its region first (the parity is read on the position with 4 empty squares, as in Egaroucid).
        if (IsLoneEmpty(p2, empties4))
        {
            (p2, p0) = (p0, p2);
        }
        else if (IsLoneEmpty(p1, empties4))
        {
            (p1, p0) = (p0, p1);
        }

        ulong p = own, o = opponent;
        int v = -EndInfinity;
        int pol = 1;
        do
        {
            _nodes++;
            ulong flips;
            if ((FlipNeighbours[p0] & o) != 0 && (flips = Flips(p, o, p0)) != 0)
            {
                v = Last2(o ^ flips, p ^ flips ^ (1UL << p0), alpha, p1, p2);
                if (alpha < v)
                {
                    return v * pol;
                }
            }

            int g;
            if ((FlipNeighbours[p1] & o) != 0 && (flips = Flips(p, o, p1)) != 0)
            {
                g = Last2(o ^ flips, p ^ flips ^ (1UL << p1), alpha, p2, p0);
                if (alpha < g)
                {
                    return g * pol;
                }

                v = Math.Max(v, g);
            }

            if ((FlipNeighbours[p2] & o) != 0 && (flips = Flips(p, o, p2)) != 0)
            {
                g = Last2(o ^ flips, p ^ flips ^ (1UL << p2), alpha, p1, p0);
                return Math.Max(v, g) * pol;
            }

            if (v > -EndInfinity)
            {
                return v * pol;
            }

            (p, o) = (o, p);
            alpha = -alpha - 1;
        }
        while ((pol = -pol) < 0);

        return ScoreWithOddEmpties(own, 3);
    }

    private int Last2(ulong own, ulong opponent, int alpha, int p0, int p1)
    {
        _nodes++;
        int v;
        ulong flips;
        if ((FlipNeighbours[p0] & opponent) != 0 && (flips = Flips(own, opponent, p0)) != 0)
        {
            v = Last1(opponent ^ flips, alpha, p1);
            if (v > alpha && (FlipNeighbours[p1] & opponent) != 0 && (flips = Flips(own, opponent, p1)) != 0)
            {
                v = Math.Min(v, Last1(opponent ^ flips, alpha, p0));
            }
        }
        else if ((FlipNeighbours[p1] & opponent) != 0 && (flips = Flips(own, opponent, p1)) != 0)
        {
            v = Last1(opponent ^ flips, alpha, p0);
        }
        else
        {
            // The side to move passes; the other side plays.
            _nodes++;
            alpha = -alpha - 1;
            if ((flips = Flips(opponent, own, p0)) != 0)
            {
                v = Last1(own ^ flips, alpha, p1);
                if (v > alpha && (flips = Flips(opponent, own, p1)) != 0)
                {
                    v = Math.Min(v, Last1(own ^ flips, alpha, p0));
                }
            }
            else if ((flips = Flips(opponent, own, p1)) != 0)
            {
                v = Last1(own ^ flips, alpha, p0);
            }
            else
            {
                v = ScoreWithEmpties(own, 2);
            }

            v = -v;
        }

        return v;
    }

    // own is the side to move; every square but the empty one that is not own's is the opponent's.
    private int Last1(ulong own, int alpha, int square)
    {
        _nodes++;
        int flipped = LastFlipCount(own, square);
        int score = 2 * (BitOperations.PopCount(own) + flipped + 1) - 64;
        if (flipped == 0)
        {
            // own cannot play there: the opponent may, or the square stays empty.
            _nodes++;
            int passScore = score - 2;
            if (score <= 0)
            {
                score = passScore;
            }

            if (score > alpha)
            {
                flipped = LastFlipCount(~own, square);
                if (flipped != 0)
                {
                    score = passScore - 2 * flipped;
                }
            }
        }

        return score;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static int LastFlipCount(ulong own, int square)
    {
        ulong bit = 1UL << square;
        return BitOperations.PopCount(Flips(own & ~bit, ~own & ~bit, square));
    }

    // ---- Helpers ----

    // The stable-disc cutoff of a null-window search: int.MinValue if it does not apply.
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static int StabilityCut(ulong own, ulong opponent, int alpha, int empties)
    {
        if (alpha >= StabilityThresholdNws[64 - empties])
        {
            int limit = 64 - 2 * BitOperations.PopCount(StableDiscs.Of(opponent, own));
            if (limit <= alpha)
            {
                return limit;
            }
        }

        return int.MinValue;
    }

    // Fills the moves with their flips; true if one of them takes every opponent disc (the game is won 64-0).
    private static bool FillEndMoves(ulong own, ulong opponent, ulong legal, Span<EndMove> moves)
    {
        for (int i = 0; legal != 0; i++, legal &= legal - 1)
        {
            int square = BitOperations.TrailingZeroCount(legal);
            ulong flips = Flips(own, opponent, square);
            if (flips == opponent)
            {
                return true;
            }

            moves[i] = new EndMove { Square = square, Flips = flips, Replies = LegalUndefined };
        }

        return false;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static (ulong Own, ulong Opponent) Child(ulong own, ulong opponent, in EndMove move) =>
        (opponent ^ move.Flips, own ^ move.Flips ^ (1UL << move.Square));

    // Moves the best of moves[start..] to start; the first of equal values wins (Egaroucid swap_next_best_move).
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static void SwapNextBest(Span<EndMove> moves, int start)
    {
        int top = start;
        for (int i = start + 1; i < moves.Length; i++)
        {
            if (moves[i].Value > moves[top].Value)
            {
                top = i;
            }
        }

        if (top != start)
        {
            (moves[start], moves[top]) = (moves[top], moves[start]);
        }
    }
}
