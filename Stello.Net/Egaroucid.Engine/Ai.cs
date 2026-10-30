// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/ai.hpp and time_management.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto
// Yamana, GPL-3.0-or-later. Single-threaded for now: lazy SMP and YBWC come in phase 6 of Plans/egaroucid-port-plan.md.
// In time mode, the extra pondering of ai_time_limit (ai_get_values, ai_align_move_levels, request_more_time) is not
// ported; see Plans/egaroucid-port-findings.md.

using System.Diagnostics;
using System.Globalization;

namespace Stello.Egaroucid;

/// <summary>The search driver (ai.hpp): level → depth and selectivity, book, clog search, iterative deepening.</summary>
internal static class Ai
{
    private const int IDSEARCH_ENDSEARCH_PRESEARCH_OFFSET = 10;
    private const int IDSEARCH_ENDSEARCH_PRESEARCH_OFFSET_TIMELIMIT = 12;
    private const int AI_TL_EARLY_BREAK_THRESHOLD = 5;

    /// <summary>
    /// The best move at <paramref name="level"/> without the book (ai.hpp <c>ai</c> with use_book = false). With
    /// <paramref name="log"/>, each iteration is reported as Egaroucid's log line (without time and speed).
    /// </summary>
    public static SearchResult Search(SearchContext context, Board board, int level, bool useMultiThread, Action<string>? log = null) =>
        AiCommon(context, board, -Common.SCORE_MAX, Common.SCORE_MAX, level, book: null, bookAccLevel: 0, random: null,
            useMultiThread, board.GetLegal(), SearchConst.TIME_LIMIT_INF, new SearchingFlag(), log);

    /// <summary>
    /// The best move with <paramref name="remainingTimeMsec"/> left for the rest of the game (ai.hpp
    /// <c>ai_time_limit</c>): the time for this move comes from <see cref="CalcTimeLimitPly"/>.
    /// </summary>
    public static SearchResult SearchTimePerGame(SearchContext context, Board board, ulong remainingTimeMsec, Book? book, int bookAccLevel, Random? random, SearchingFlag searching)
    {
        ulong timeLimit = CalcTimeLimitPly(board, remainingTimeMsec);
        return SearchTimePerMove(context, board, timeLimit, book, bookAccLevel, random, searching);
    }

    /// <summary>
    /// The best move within <paramref name="timeLimitMsec"/> (<c>ai_common</c> at level 60 with a time limit). Unlike
    /// C++, it always gives a legal move when there is one: if the search was stopped before its first iteration (for
    /// example during the clog search), a level-1 search chooses the move.
    /// </summary>
    public static SearchResult SearchTimePerMove(SearchContext context, Board board, ulong timeLimitMsec, Book? book, int bookAccLevel, Random? random, SearchingFlag searching)
    {
        SearchResult result = AiCommon(context, board, -Common.SCORE_MAX, Common.SCORE_MAX, Level.MAX_LEVEL, book, bookAccLevel, random,
            useMultiThread: false, board.GetLegal(), timeLimitMsec, searching, log: null);
        if (result.Policy is < 0 or >= Common.HW2 && board.GetLegal() != 0)
        {
            SearchResult quick = Search(context, board, 1, useMultiThread: false);
            quick.Nodes += result.Nodes;
            quick.ClogNodes += result.ClogNodes;
            quick.Time += result.Time;
            return quick;
        }

        return result;
    }

    /// <summary>
    /// <c>ai_common</c>: handles a pass, then plays from the book if it can, otherwise searches. With book accuracy 0,
    /// the moves that are not in the book are searched too, and one of them is played if it beats the book.
    /// </summary>
    /// <remarks>
    /// As in C++, <paramref name="useLegal"/> is taken before the pass: for a position where the side to move must pass,
    /// nothing is searched and the result has no move. Callers pass first, as Egaroucid's own do.
    /// </remarks>
    public static SearchResult AiCommon(SearchContext context, Board board, int alpha, int beta, int level, Book? book, int bookAccLevel, Random? random,
        bool useMultiThread, ulong useLegal, ulong timeLimit, SearchingFlag searching, Action<string>? log)
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

        BookValue bookResult = book is not null
            ? book.GetRandom(board, bookAccLevel, useLegal, random ?? Random.Shared)
            : new BookValue(-1, -Common.INF);
        Level.GetLevel(level, board.NDiscs() - 4, out _, out int depth, out int mpcLevel);
        if (book is not null && bookResult.Policy is >= 0 and < Common.HW2)
        {
            if (bookAccLevel == 0)
            {
                foreach (BookValue move in book.GetAllMovesWithValue(board))
                {
                    useLegal &= ~(1UL << move.Policy);
                }

                if (useLegal != 0)
                {
                    int nAlpha = bookResult.Value;
                    SearchResult additional = TreeSearchLegal(context, board, nAlpha, nAlpha + 1, depth, mpcLevel, useLegal, useMultiThread, timeLimit, searching, log);
                    if (additional.Value <= nAlpha)
                    {
                        // As in C++, the depth of the extra search overwrites SEARCH_BOOK.
                        res = new SearchResult
                        {
                            Level = Level.LEVEL_TYPE_BOOK,
                            Policy = bookResult.Policy,
                            Value = valueSign * bookResult.Value,
                            ClogNodes = additional.ClogNodes,
                            ClogTime = additional.ClogTime,
                            Nodes = additional.Nodes,
                            Depth = additional.Depth,
                            Time = additional.Time,
                            Nps = additional.Nps,
                            IsEndSearch = false,
                            Probability = 100,
                        };
                    }
                    else
                    {
                        res = TreeSearchLegal(context, board, nAlpha, beta, depth, mpcLevel, useLegal, useMultiThread, timeLimit, searching, log);
                        res.Time += additional.Time;
                        res.Nodes += additional.Nodes;
                        res.ClogNodes += additional.ClogNodes;
                        res.ClogTime += additional.ClogTime;
                        res.Nps = res.Nodes * 1000UL / Math.Max(1UL, res.Time);
                    }
                }
                else
                {
                    res = BookResult(bookResult, valueSign);
                }
            }
            else
            {
                res = BookResult(bookResult, valueSign);
            }
        }
        else
        {
            res = TreeSearchLegal(context, board, alpha, beta, depth, mpcLevel, useLegal, useMultiThread, timeLimit, searching, log);
            res.Level = level;
        }

        res.Value *= valueSign;
        return res;
    }

    private static SearchResult BookResult(BookValue bookResult, int valueSign) => new()
    {
        Level = Level.LEVEL_TYPE_BOOK,
        Policy = bookResult.Policy,
        Value = valueSign * bookResult.Value,
        Depth = SearchConst.SEARCH_BOOK,
        Nps = 0,
        IsEndSearch = false,
        Probability = 100,
    };

    /// <summary>The clog search (when selective or timed) and then iterative deepening (<c>tree_search_legal</c>).</summary>
    public static SearchResult TreeSearchLegal(SearchContext context, Board board, int alpha, int beta, int depth, int mpcLevel, ulong useLegal, bool useMultiThread,
        ulong timeLimit, SearchingFlag searching, Action<string>? log)
    {
        var res = new SearchResult();
        depth = Math.Min(Common.HW2 - board.NDiscs(), depth);
        bool useTimeLimit = timeLimit != SearchConst.TIME_LIMIT_INF;
        List<ClogResult> clogs = [];
        ulong clogTime = 0;
        if (mpcLevel != Level.MPC_100 || useTimeLimit)
        {
            long start = Stopwatch.GetTimestamp();
            int clogDepth = useTimeLimit ? ClogSearch.CLOG_SEARCH_MAX_DEPTH : Math.Min(depth, ClogSearch.CLOG_SEARCH_MAX_DEPTH);
            clogs = ClogSearch.FirstClogSearch(context, board, out ulong clogNodes, clogDepth, useLegal, searching);
            clogTime = ElapsedMsec(start);
            res.ClogNodes = clogNodes;
            res.ClogTime = clogTime;
        }

        if (useLegal != 0)
        {
            if (useTimeLimit)
            {
                ulong timeLimitProc = timeLimit > clogTime ? timeLimit - clogTime : 1;
                IterativeDeepeningSearchTimeLimit(context, board, alpha, beta, clogs, useLegal, useMultiThread, res, timeLimitProc, searching);
            }
            else
            {
                IterativeDeepeningSearch(context, board, alpha, beta, depth, mpcLevel, clogs, useLegal, useMultiThread, res, searching, log);
            }
        }

        return res;
    }

    /// <summary>Iterative deepening to a fixed depth and selectivity (<c>iterative_deepening_search</c>, without lazy SMP).</summary>
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
                result.Value = result.Value != SearchConst.SCORE_UNDEFINED && !mainIsEndSearch
                    ? SmoothValue(result.Value, idValue)
                    : idValue;
                result.Policy = idPolicy;
                result.Depth = mainDepth;
                result.IsEndSearch = mainIsEndSearch;
                result.Probability = (int)Level.SelectivityPercentage[mainMpcLevel];
            }

            result.Time = ElapsedMsec(start);
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

    /// <summary>
    /// Iterative deepening until the time is up (<c>iterative_deepening_search_time_limit</c>). C++ runs each iteration
    /// in its own thread and stops waiting when the time is up; here the iteration's flag has a deadline instead, and an
    /// iteration that was stopped does not count. The first iteration always runs.
    /// </summary>
    public static void IterativeDeepeningSearchTimeLimit(SearchContext context, Board board, int alpha, int beta, IReadOnlyList<ClogResult> clogs, ulong useLegal, bool useMultiThread, SearchResult result, ulong timeLimit, SearchingFlag searching)
    {
        long start = Stopwatch.GetTimestamp();
        result.Value = SearchConst.SCORE_UNDEFINED;
        int mainDepth = 1;
        int mainMpcLevel = Level.MPC_100;
        int maxDepth = Common.HW2 - board.NDiscs();
        bool policyChangedBefore = true;
        while (context.GlobalSearching && searching.Value && (ElapsedMsec(start) < timeLimit || mainDepth <= 1))
        {
            bool mainIsEndSearch = false;
            if (mainDepth >= maxDepth)
            {
                mainIsEndSearch = true;
                mainDepth = maxDepth;
            }

            var mainSearch = new Search(context, board, mainMpcLevel, useMultiThread, isPresearch: false);
            ulong limitThisSearch = Remaining(timeLimit, ElapsedMsec(start));
            (bool success, (int Value, int Move) idResult) = RunTimed(limitThisSearch, searching, flag =>
                MidSearch.FirstNegaScoutLegal(mainSearch, alpha, beta, mainDepth, mainIsEndSearch, clogs, useLegal, flag));
            result.Nodes += mainSearch.NNodes;
            result.Time = ElapsedMsec(start);
            result.Nps = result.Nodes * 1000UL / Math.Max(1UL, result.Time);
            if (success)
            {
                result.Value = result.Value != SearchConst.SCORE_UNDEFINED && !mainIsEndSearch
                    ? SmoothValue(result.Value, idResult.Value)
                    : idResult.Value;
                bool policyChanged = result.Policy != idResult.Move;
                result.Policy = idResult.Move;
                result.Depth = mainDepth;
                result.IsEndSearch = mainIsEndSearch;
                result.Probability = (int)Level.SelectivityPercentage[mainMpcLevel];

                // At depth 29-30 with a stable best move, a null-window search of the other moves may end the search early.
                ulong legalWithoutBestMove = useLegal ^ (1UL << result.Policy);
                if (!mainIsEndSearch && mainDepth is >= 29 and <= 30 && maxDepth >= 44 && !policyChanged && !policyChangedBefore &&
                    mainMpcLevel == Level.MPC_74 && legalWithoutBestMove != 0)
                {
                    int nwsAlpha = result.Value - AI_TL_EARLY_BREAK_THRESHOLD;
                    if (nwsAlpha >= -Common.SCORE_MAX)
                    {
                        var nwsSearch = new Search(context, board, mainMpcLevel, useMultiThread, isPresearch: false);
                        int depthNow = mainDepth;
                        (bool nwsSuccess, (int Value, int Move) nwsResult) = RunTimed(Remaining(timeLimit, ElapsedMsec(start)), searching, flag =>
                            MidSearch.FirstNegaScoutLegal(nwsSearch, nwsAlpha, nwsAlpha + 1, depthNow, mainIsEndSearch, clogs, legalWithoutBestMove, flag));
                        result.Nodes += nwsSearch.NNodes;
                        result.Time = ElapsedMsec(start);
                        result.Nps = result.Nodes * 1000UL / Math.Max(1UL, result.Time);
                        if (nwsSuccess && nwsResult.Value <= nwsAlpha)
                        {
                            break;
                        }
                    }
                }

                policyChangedBefore = policyChanged;
            }

            if (mainDepth < maxDepth - IDSEARCH_ENDSEARCH_PRESEARCH_OFFSET_TIMELIMIT)
            {
                mainDepth += mainDepth <= 15 && mainDepth < maxDepth - 3 ? 3 : 1;
                if (mainDepth > 13 && mainMpcLevel == Level.MPC_100)
                {
                    mainMpcLevel = Level.MPC_74;
                }
            }
            else if (mainDepth < maxDepth)
            {
                mainDepth = maxDepth;
                mainMpcLevel = Level.MPC_74;
            }
            else if (mainMpcLevel < Level.MPC_100)
            {
                ++mainMpcLevel;
            }
            else
            {
                break;
            }
        }
    }

    /// <summary>
    /// The time for this move, in milliseconds, from the time left for the game (time_management.hpp
    /// <c>calc_time_limit_ply</c>). It keeps a margin, plans the midgame moves, and allows a large share when the
    /// remaining empty squares look solvable in the time. Its speed constants are those of a many-threaded C++ build.
    /// </summary>
    public static ulong CalcTimeLimitPly(in Board board, ulong remainingTimeMsec)
    {
        const double offset = 400, offsetBase = 5000;
        int nEmpties = Common.HW2 - board.NDiscs();
        double remainingMoves = (nEmpties + 1) / 2.0;
        ulong margin = remainingTimeMsec;
        if (remainingTimeMsec > offset * remainingMoves + offsetBase)
        {
            margin -= (ulong)(offset * remainingMoves + offsetBase);
        }

        double completeUseTime = margin * 0.9;
        double completeSearchDepth = Math.Log(completeUseTime / 1000.0 * 7.0e8 / 0.60) / 0.75;
        double endgameUseTime = margin * 0.15;
        double endgameSearchDepth = Math.Log(endgameUseTime / 1000.0 * 3.5e8 / 0.05) / 0.62;
        double remainingMovesProc = 0;
        if (margin < remainingTimeMsec)
        {
            if (remainingMoves >= 30 / 2)
            {
                remainingMovesProc += (remainingMoves - 30 / 2) * 1.4;
            }

            if (remainingMoves >= 40 / 2)
            {
                remainingMovesProc += (remainingMoves - 40 / 2) * 0.2;
            }
        }
        else
        {
            remainingMovesProc += (remainingMoves - 26 / 2) * 1.2;
        }

        remainingMovesProc = Math.Max(2.0, remainingMovesProc);
        ulong midgameUseTime = Math.Max(1UL, (ulong)(margin / remainingMovesProc));
        if (nEmpties <= completeSearchDepth)
        {
            return Math.Max((ulong)completeUseTime, midgameUseTime);
        }

        return nEmpties <= endgameSearchDepth ? Math.Max((ulong)endgameUseTime, midgameUseTime) : midgameUseTime;
    }

    /// <summary>The square name of a policy, as C++ <c>idx_to_coord</c> prints it ("??" for none).</summary>
    public static string PolicyName(int policy) => policy is >= 0 and < Common.HW2 ? Common.IdxToCoord(policy) : "??";

    // (0.9 × previous + 1.1 × new) / 2, rounded as C's round (halves away from zero).
    private static int SmoothValue(int previous, int next) =>
        (int)Math.Round((0.9 * previous + 1.1 * next) / 2.0, MidpointRounding.AwayFromZero);

    private static ulong ElapsedMsec(long start) => (ulong)Stopwatch.GetElapsedTime(start).TotalMilliseconds;

    private static ulong Remaining(ulong limit, ulong elapsed) => limit <= elapsed ? 0 : limit - elapsed;

    /// <summary>Runs <paramref name="search"/> with a flag that stops it after <paramref name="limitMsec"/>; success if it was not stopped.</summary>
    private static (bool Success, T Result) RunTimed<T>(ulong limitMsec, SearchingFlag parent, Func<SearchingFlag, T> search)
    {
        long deadline = Stopwatch.GetTimestamp() + (long)(Math.Min(limitMsec, (ulong)int.MaxValue) * (Stopwatch.Frequency / 1000.0));
        var flag = new SearchingFlag(parent, Math.Max(1, deadline));
        T result = search(flag);
        bool success = flag.Value && Stopwatch.GetTimestamp() < deadline;
        return (success, result);
    }
}
