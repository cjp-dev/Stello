// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/clogsearch.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana,
// GPL-3.0-or-later.

namespace Stello.Egaroucid;

/// <summary>
/// The clog search (clogsearch.hpp): a full search of the next few moves that finds root moves after which the game
/// ends within that depth (for example by a wipe-out), with their exact values, so the selective search cannot miss
/// them. The C++ code splits it over threads when some are idle; on one thread it is this serial search.
/// </summary>
internal static class ClogSearch
{
    public const int CLOG_NOT_FOUND = -127;
    public const int CLOG_SEARCH_MAX_DEPTH = 5;

    private static int Search(Search search, int depth, SearchingFlag searching)
    {
        if (!search.Context.GlobalSearching)
        {
            return CLOG_NOT_FOUND;
        }

        ++search.NNodes;
        if (depth == 0)
        {
            return search.Board.IsEnd() ? search.Board.ScorePlayer() : CLOG_NOT_FOUND;
        }

        int res = CLOG_NOT_FOUND;
        ulong legal = search.Board.GetLegal();
        if (legal == 0)
        {
            search.Pass();
            res = search.Board.GetLegal() == 0 ? search.Board.ScorePlayer() : Search(search, depth, searching);
            search.Pass();
            return res != CLOG_NOT_FOUND ? -res : CLOG_NOT_FOUND;
        }

        int canput = Bits.PopCount(legal);
        Span<FlipValue> moveList = stackalloc FlipValue[SearchConst.MAX_N_BRANCHES];
        int idx = 0;
        for (int cell = Bits.FirstBit(ref legal); legal != 0; cell = Bits.NextBit(ref legal))
        {
            moveList[idx].Reset();
            moveList[idx].Flip.Calc(search.Board.Player, search.Board.Opponent, cell);
            if (moveList[idx].Flip.Flipped == search.Board.Opponent)
            {
                return Common.SCORE_MAX;
            }

            ++idx;
        }

        Span<int> moves = [SearchConst.MOVE_UNDEFINED, SearchConst.MOVE_UNDEFINED];
        search.Context.TranspositionTable.GetMovesAnyLevel(search.Board, search.Board.Hash(), moves);
        MoveOrdering.MoveListEvaluate(search, moveList, canput, moves, depth, -Common.SCORE_MAX, Common.SCORE_MAX, searching);
        bool uncertainValueFound = false;
        int beta = Common.HW2 - 2 * Bits.PopCount(Stability.CalcStability(search.Board.Opponent, search.Board.Player));

        // The parallel version of C++ (USE_PARALLEL_CLOG_SEARCH) with no idle thread: its loop does not test
        // global_searching, and it searches every move itself.
        for (int moveIdx = 0; moveIdx < canput && res < beta && searching.Value; ++moveIdx)
        {
            MoveOrdering.SwapNextBestMove(moveList, moveIdx, canput);
            search.Move(moveList[moveIdx].Flip);
            int g = Search(search, depth - 1, searching);
            if (g != CLOG_NOT_FOUND)
            {
                res = Math.Max(res, -g);
            }
            else
            {
                uncertainValueFound = true;
            }

            search.Undo(moveList[moveIdx].Flip);
        }

        if (res < beta && (!searching.Value || !search.Context.GlobalSearching))
        {
            return CLOG_NOT_FOUND;
        }

        if (uncertainValueFound && res < beta)
        {
            res = CLOG_NOT_FOUND;
        }

        return res;
    }

    /// <summary>The root moves in <paramref name="legal"/> with a clog value (<c>first_clog_search</c>).</summary>
    public static List<ClogResult> FirstClogSearch(SearchContext context, in Board board, out ulong nNodes, int depth, ulong legal, SearchingFlag searching)
    {
        var search = new Search(context, board, Level.MPC_100, useMultiThread: true, isPresearch: false);
        var res = new List<ClogResult>();
        var flip = new Flip();
        for (int cell = Bits.FirstBit(ref legal); legal != 0 && searching.Value && context.GlobalSearching; cell = Bits.NextBit(ref legal))
        {
            flip.Calc(search.Board.Player, search.Board.Opponent, cell);
            search.Move(flip);
            int g = Search(search, depth - 1, searching);
            if (g != CLOG_NOT_FOUND)
            {
                res.Add(new ClogResult(cell, -g));
            }

            search.Undo(flip);
        }

        nNodes = search.NNodes;
        return res;
    }
}
