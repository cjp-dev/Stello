// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/ai.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later.
// Only the single-threaded fixed-level path is ported so far: lazy SMP and YBWC (phase 6), the time limit and the
// book (phase 4) of Plans/egaroucid-port-plan.md come later.

using System.Diagnostics;
using System.Globalization;

namespace Stello.Egaroucid;

/// <summary>The search driver (ai.hpp): level → depth and selectivity, clog search, iterative deepening.</summary>
internal static class Ai
{
    private const int IDSEARCH_ENDSEARCH_PRESEARCH_OFFSET = 10;

    /// <summary>
    /// The best move at <paramref name="level"/> without the book (ai.hpp <c>ai</c> with use_book = false). With
    /// <paramref name="log"/>, each iteration is reported as Egaroucid's log line (without time and speed).
    /// </summary>
    public static SearchResult Search(SearchContext context, Board board, int level, bool useMultiThread, Action<string>? log = null)
    {
        var searching = new SearchingFlag();
        return AiCommon(context, board, -Common.SCORE_MAX, Common.SCORE_MAX, level, useMultiThread, board.GetLegal(), searching, log);
    }

    /// <summary><c>ai_common</c> without the book.</summary>
    public static SearchResult AiCommon(SearchContext context, Board board, int alpha, int beta, int level, bool useMultiThread, ulong useLegal, SearchingFlag searching, Action<string>? log)
    {
        SearchResult res;
        int valueSign = 1;
        if (board.GetLegal() == 0)
        {
            board.Pass();
            if (board.GetLegal() == 0)
            {
                return new SearchResult
                {
                    Level = Level.MAX_LEVEL,
                    Policy = 64,
                    Value = -board.ScorePlayer(),
                    Depth = 0,
                    Nps = 0,
                    IsEndSearch = true,
                    Probability = 100,
                };
            }

            valueSign = -1;
        }

        Level.GetLevel(level, board.NDiscs() - 4, out _, out int depth, out int mpcLevel);
        res = TreeSearchLegal(context, board, alpha, beta, depth, mpcLevel, useLegal, useMultiThread, searching, log);
        res.Level = level;
        res.Value *= valueSign;
        return res;
    }

    /// <summary>The clog search (when selective) and then iterative deepening (<c>tree_search_legal</c>).</summary>
    public static SearchResult TreeSearchLegal(SearchContext context, Board board, int alpha, int beta, int depth, int mpcLevel, ulong useLegal, bool useMultiThread, SearchingFlag searching, Action<string>? log)
    {
        var res = new SearchResult();
        depth = Math.Min(Common.HW2 - board.NDiscs(), depth);
        List<ClogResult> clogs = [];
        if (mpcLevel != Level.MPC_100)
        {
            long start = Stopwatch.GetTimestamp();
            int clogDepth = Math.Min(depth, ClogSearch.CLOG_SEARCH_MAX_DEPTH);
            clogs = ClogSearch.FirstClogSearch(context, board, out ulong clogNodes, clogDepth, useLegal, searching);
            res.ClogNodes = clogNodes;
            res.ClogTime = (ulong)Stopwatch.GetElapsedTime(start).TotalMilliseconds;
        }

        if (useLegal != 0)
        {
            IterativeDeepeningSearch(context, board, alpha, beta, depth, mpcLevel, clogs, useLegal, useMultiThread, res, searching, log);
        }

        return res;
    }

    /// <summary>Iterative deepening (<c>iterative_deepening_search</c>, without lazy SMP).</summary>
    public static void IterativeDeepeningSearch(SearchContext context, Board board, int alpha, int beta, int depth, int mpcLevel, IReadOnlyList<ClogResult> clogs, ulong useLegal, bool useMultiThread, SearchResult result, SearchingFlag searching, Action<string>? log)
    {
        long start = Stopwatch.GetTimestamp();
        result.Value = SearchConst.SCORE_UNDEFINED;
        int mainDepth = 1;
        int mainMpcLevel = mpcLevel;
        int maxDepth = Common.HW2 - board.NDiscs();
        depth = Math.Min(depth, maxDepth);
        bool isEndSearch = depth == maxDepth;
        if (isEndSearch)
        {
            mainMpcLevel = Level.MPC_74;
        }

        while (mainDepth <= depth && mainMpcLevel <= mpcLevel && context.GlobalSearching && searching.Value)
        {
            bool mainIsEndSearch = false;
            if (mainDepth >= maxDepth)
            {
                mainIsEndSearch = true;
                mainDepth = maxDepth;
            }

            bool isLastSearch = mainDepth == depth && mainMpcLevel == mpcLevel;
            var mainSearch = new Search(context, board, mainMpcLevel, useMultiThread, !isLastSearch);
            (int idValue, int idPolicy) = MidSearch.FirstNegaScoutLegal(mainSearch, alpha, beta, mainDepth, mainIsEndSearch, clogs, useLegal, searching);
            result.Nodes += mainSearch.NNodes;
            if (searching.Value)
            {
                if (result.Value != SearchConst.SCORE_UNDEFINED && !mainIsEndSearch)
                {
                    double nValue = (0.9 * result.Value + 1.1 * idValue) / 2.0;
                    result.Value = (int)Math.Round(nValue, MidpointRounding.AwayFromZero);
                }
                else
                {
                    result.Value = idValue;
                }

                result.Policy = idPolicy;
                result.Depth = mainDepth;
                result.IsEndSearch = mainIsEndSearch;
                result.Probability = (int)Level.SelectivityPercentage[mainMpcLevel];
            }

            result.Time = (ulong)Stopwatch.GetElapsedTime(start).TotalMilliseconds;
            result.Nps = result.Nodes * 1000UL / Math.Max(1UL, result.Time);
            log?.Invoke(string.Create(CultureInfo.InvariantCulture,
                $"{(isLastSearch ? "main" : "pre")} {(mainIsEndSearch ? "end" : "mid")} depth {result.Depth}@{Level.SelectivityPercentage[mainMpcLevel]}% value {result.Value} (raw {idValue}) policy {PolicyName(idPolicy)} n_worker 0 n_nodes {result.Nodes}"));

            if (isEndSearch && mainDepth >= depth - IDSEARCH_ENDSEARCH_PRESEARCH_OFFSET)
            {
                if (mainDepth < depth)
                {
                    mainDepth = depth;
                    mainMpcLevel = depth <= 27 && mpcLevel >= Level.MPC_88 ? Level.MPC_88 : Level.MPC_74;
                }
                else if (mainMpcLevel < mpcLevel)
                {
                    if ((mainMpcLevel >= Level.MPC_74 && mpcLevel > Level.MPC_74 && depth <= 22) ||
                        (mainMpcLevel >= Level.MPC_88 && mpcLevel > Level.MPC_88 && depth <= 25) ||
                        (mainMpcLevel >= Level.MPC_93 && mpcLevel > Level.MPC_93 && depth <= 29) ||
                        (mainMpcLevel >= Level.MPC_98 && mpcLevel > Level.MPC_98))
                    {
                        mainMpcLevel = mpcLevel;
                    }
                    else
                    {
                        ++mainMpcLevel;
                    }
                }
                else
                {
                    break;
                }
            }
            else if (mainDepth <= 15 && mainDepth < depth - 3)
            {
                mainDepth += 3;
            }
            else
            {
                ++mainDepth;
            }
        }
    }

    /// <summary>The square name of a policy, as C++ <c>idx_to_coord</c> prints it ("??" for none).</summary>
    public static string PolicyName(int policy) => policy is >= 0 and < Common.HW2 ? Common.IdxToCoord(policy) : "??";
}
