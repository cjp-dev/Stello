using System.Diagnostics;
using System.Numerics;
using Stello.Engine.Evaluation;
using Stello.Engine.Search;

namespace Stello.Engine;

/// <summary>
/// Stello 2's search: the midgame search ported from Minmax.cpp and Kontrol.cpp (iterative deepening with a hash
/// table, response killers and selective search), with the trained pattern evaluation, and the endgame solver
/// adapted from Egaroucid (Endgame/SearchEngine.Endgame.cs). An instance runs one search at a time.
/// </summary>
public sealed partial class SearchEngine
{
    private const int Infinity = 32767;
    private const int RootWindow = 32664;

    // C++: the endgame solver takes over when (allway - varlook) <= 7.
    private const int EndgameDistance = 7;

    // No Multi-ProbCut near won or lost scores.
    private const int ProbCutScoreLimit = 32000;

    /// <summary>A finished game in the midgame search scores this plus the disc difference (C++: the same).</summary>
    internal const int WinScore = 32600;

    // Quadrants a1-d4, e1-h4, a5-d8, e5-h8; squares in regions with an odd number of empties are tried first.
    private static readonly ulong[] Quadrants = [0x0000_0000_0F0F_0F0FUL, 0x0000_0000_F0F0_F0F0UL, 0x0F0F_0F0F_0000_0000UL, 0xF0F0_F0F0_0000_0000UL];

    // The pattern evaluation's scores in the search: 1/100 disc, close to Stello 1's evaluation scale.
    private const int PatternUnitsPerDisc = 100;

    private readonly TranspositionTable _midgameTable;
    private readonly TranspositionTable _endgameTable;
    private readonly MoveOrdering _ordering = new();
    private readonly Stopwatch _clock = new();

    private long _nodes;
    private long _evaluations;
    private long _hardLimitMs;
    private int _hashGetHeight;
    private int _hashPutHeight;
    private CancellationToken _cancel;
    private CancellationToken _moveNow;
    private IProgress<SearchInfo>? _progress;
    private long _lastHeartbeatMs;
    private int _reportDepth;
    private int _reportMove = -1;
    private int _reportBestMove = -1;
    private int _reportScore;
    private ScoreKind _reportKind;

    public int HashBits { get; }

    internal SearchOptions Options { get; }

    /// <summary>Called after each finished midgame iteration with its depth and score (for the Multi-ProbCut fit).</summary>
    internal Action<int, int>? IterationDone { get; set; }

    private readonly EvalWeights _pattern;
    private readonly int? _softTimePercent;

    // Multi-ProbCut (Egaroucid's multi_probcut.hpp, with an error model fitted on Stello 2's own searches). From depth 3
    // (look 2); a cut needs the shallow search to be t standard deviations outside the window.
    internal const int DefaultProbCutPercent = 100;
    private const int ProbCutMinLook = 2;
    private const int ProbCutPreCheckOffset = 4 * PatternUnitsPerDisc;
    private readonly int[,]? _probCutErrors;
    private bool _inProbCut;

    // From look 4 (5 plies) the moves are ordered by evaluation, and the children's stored bounds are tried first (ETC).
    private const int EvaluationOrderMinLook = 4;
    private const int MidgameEtcMinLook = 4;

    /// <param name="hashBits">Each of the two hash tables gets 2^hashBits slots of two entries (C++: HASHSIZE 19, one entry).</param>
    public SearchEngine(int hashBits = 19)
        : this(hashBits, SearchOptions.Default)
    {
    }

    internal SearchEngine(int hashBits, SearchOptions options)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(hashBits, 10);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(hashBits, 26);
        ArgumentNullException.ThrowIfNull(options);
        HashBits = hashBits;
        Options = options;
        _pattern = options.Pattern ?? EvalWeights.Default;
        _softTimePercent = options.SoftTimePercent;
        _probCutErrors = options.Selective ? ProbCutErrors((options.ProbCutPercent ?? DefaultProbCutPercent) / 100.0) : null;
        _midgameTable = new TranspositionTable(hashBits);
        _endgameTable = new TranspositionTable(hashBits);
    }

    public void ClearHash()
    {
        _midgameTable.Clear();
        _endgameTable.Clear();
        _localTable.Clear();
    }

    /// <summary>Finds the best move for <paramref name="player"/> (C++: getcomputer).</summary>
    /// <param name="cancellationToken">Stops the search and throws <see cref="OperationCanceledException"/>.</param>
    /// <param name="moveNowToken">Stops the search and returns the best move found so far.</param>
    /// <param name="onlyMoves">
    /// Search only these moves, even if just one is left (C++ calclib: the best move not yet in the book).
    /// </param>
    public SearchResult Search(
        Board board,
        Player player,
        SearchLimits limits,
        IProgress<SearchInfo>? progress = null,
        CancellationToken cancellationToken = default,
        CancellationToken moveNowToken = default,
        ulong? onlyMoves = null)
    {
        ArgumentNullException.ThrowIfNull(limits);
        cancellationToken.ThrowIfCancellationRequested();

        _clock.Restart();
        _lastHeartbeatMs = 0;
        _reportMove = -1;
        _nodes = 0;
        _nextAbortCheck = 1024;
        _evaluations = 0;
        _cancel = cancellationToken;
        _moveNow = moveNowToken;
        _progress = progress;

        ulong moves = board.LegalMoves(player);
        if (onlyMoves is { } allowed)
        {
            moves &= allowed;
            if (moves == 0)
            {
                throw new ArgumentException("None of the moves to search is legal.", nameof(onlyMoves));
            }
        }

        if (moves == 0)
        {
            return Result(new Best(-1, 0, ScoreKind.None, 0));
        }

        if (BitOperations.PopCount(moves) == 1 && limits.Mode != TimeControlMode.Solve && onlyMoves is null)
        {
            return Result(new Best(BitOperations.TrailingZeroCount(moves), 0, ScoreKind.None, 0));
        }

        _ordering.ClearResponses();
        int[] rootMoves = new int[BitOperations.PopCount(moves)];
        Fill(rootMoves, moves);
        _ordering.Order(rootMoves, -1, -1, player, board);

        int empties = board.EmptyCount;
        var best = new Best(rootMoves[0], 0, ScoreKind.None, 0);
        TimeBudget budget = WithSoftPercent(TimeControl.ForMidgame(limits, empties));
        _hardLimitMs = budget.HardMs;

        try
        {
            if (limits.Mode != TimeControlMode.Solve)
            {
                for (int look = 0; empties - 1 - look > EndgameDistance; look++)
                {
                    _hashGetHeight = look - 1;
                    _hashPutHeight = look;
                    SearchRoot(board, player, look, rootMoves, ref best);
                    IterationDone?.Invoke(look + 1, best.Score);

                    if (StopIterating(limits, budget, look + 1))
                    {
                        return Result(best);
                    }
                }
            }
            else
            {
                for (int look = 0; look < SolveOrderingLooks && empties - 1 - look > EndgameDistance; look++)
                {
                    _hashGetHeight = look - 1;
                    _hashPutHeight = look;
                    SearchRoot(board, player, look, rootMoves, ref best);
                }
            }

            budget = WithSoftPercent(TimeControl.ForEndgame(limits, budget, _clock.ElapsedMilliseconds));
            _hardLimitMs = budget.HardMs;

            // Win/loss/draw first with a null window around 0; it is much faster than the exact score.
            (int wld, int wldMove) = SolveRoot(board, player, -1, 1, ScoreKind.WinLossDraw, rootMoves, ref best);
            bool keepMidgameMove = wld < 0 && best.Kind == ScoreKind.Heuristic;
            best = new Best(keepMidgameMove ? best.Move : wldMove, wld, wld == 0 ? ScoreKind.Exact : ScoreKind.WinLossDraw, empties);

            if (wld == 0 || StopIterating(limits, budget, 0))
            {
                return Result(best);
            }

            (int alpha, int beta) = wld > 0 ? (wld - 1, 65) : (-65, wld + 1);
            (int exact, int exactMove) = SolveRoot(board, player, alpha, beta, ScoreKind.Exact, rootMoves, ref best);
            best = new Best(exactMove, exact, ScoreKind.Exact, empties);
        }
        catch (SearchAbortedException)
        {
            // Time is up or "move now": keep the best move found so far.
        }

        return Result(best);
    }

    private void SearchRoot(in Board board, Player player, int look, int[] rootMoves, ref Best best)
    {
        Player opponent = player.Opponent();
        int alpha = -RootWindow;
        int beta = RootWindow;
        int bestScore = -Infinity;
        int bestMove = -1;
        Span<int> improvements = stackalloc int[rootMoves.Length];
        int improved = 0;

        for (int i = 0; i < rootMoves.Length; i++)
        {
            int move = rootMoves[i];
            _reportDepth = look + 1;
            _reportMove = move;
            _reportBestMove = bestMove >= 0 ? bestMove : best.Move;
            _reportScore = bestMove >= 0 ? bestScore : best.Score;
            _reportKind = ScoreKind.Heuristic;
            Board child = Play(board, player, move);
            int score;

            if (look == 0)
            {
                score = -EvaluateLeaf(child, opponent);
            }
            else
            {
                int childLook = look > 0 ? look - 1 : 0;
                int reply;

                // C++ zero_findmax: null window after the first move once the search is deeper than 3 plies.
                if (i == 0 || look <= 2)
                {
                    score = -Search(child, opponent, childLook, 1, move, -beta, -alpha, out reply);
                }
                else
                {
                    score = -Search(child, opponent, childLook, 1, move, -alpha - 1, -alpha, out reply);
                    if (score > alpha && score < beta)
                    {
                        score = -Search(child, opponent, childLook, 1, move, -beta, -alpha, out reply);
                    }
                }

                _ordering.RecordResponse(opponent, move, reply, refuted: score <= alpha);
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestMove = move;
                alpha = Math.Max(alpha, score);
                improvements[improved++] = move;
                best = new Best(move, score, ScoreKind.Heuristic, look + 1);
            }

            Report(look + 1, move, bestMove, bestScore, ScoreKind.Heuristic);
        }

        MoveToFront(rootMoves, improvements[..improved]);
    }

    // C++: findmax2 (findmax/findmax1 kept a move tree for ordering; the hash table does that here).
    private int Search(in Board board, Player player, int look, int ply, int previousMove, int alpha, int beta, out int bestMove)
    {
        if ((++_nodes & 1023) == 0)
        {
            CheckAbort();
        }

        bestMove = -1;
        Player opponent = player.Opponent();
        ulong own = board.Discs(player);
        ulong other = board.Discs(opponent);
        ulong moves = Bitboards.LegalMoves(own, other);

        if (moves == 0)
        {
            if (Bitboards.LegalMoves(other, own) == 0)
            {
                _evaluations++;
                return GameOverScore(own, other);
            }

            return -Search(board, opponent, look, ply, -1, -beta, -alpha, out _);
        }

        int hashMove = -1;
        bool hashGet = ply <= _hashGetHeight;
        bool hashPut = ply <= _hashPutHeight;
        int tag = (int)player;

        if (hashGet && _midgameTable.TryGet(own, other, tag, out TranspositionTable.Entry entry)
            && entry.Move >= 0 && (moves & (1UL << entry.Move)) != 0)
        {
            if (entry.Depth >= look && entry.Bound switch
                {
                    Bound.Exact => true,
                    Bound.Upper => entry.Value < alpha,
                    Bound.Lower => entry.Value >= beta,
                    _ => false,
                })
            {
                bestMove = entry.Move;
                return entry.Value;
            }

            hashMove = entry.Move;
        }

        // Multi-ProbCut: a shallow null-window search predicts that the full search falls outside the window.
        if (_probCutErrors is not null && look >= ProbCutMinLook && !_inProbCut
            && TryProbCut(board, player, look, ply, previousMove, alpha, beta, out int cut))
        {
            return cut;
        }

        int storedLook = look;
        int moveCount = BitOperations.PopCount(moves);

        // A single reply at the horizon is searched one ply deeper.
        if (moveCount == 1 && look == 0)
        {
            look = 1;
        }

        Span<int> list = stackalloc int[moveCount];
        Fill(list, moves);

        // Enhanced transposition cutoff: a child whose stored upper bound already refutes it gives a cutoff here.
        if (look >= MidgameEtcMinLook && ply + 1 <= _hashGetHeight)
        {
            foreach (int move in list)
            {
                Board child = Play(board, player, move);
                if (_midgameTable.TryGet(child.Discs(opponent), child.Discs(player), (int)opponent, out TranspositionTable.Entry childEntry)
                    && childEntry.Depth >= look - 1 && childEntry.Bound != Bound.Lower && -childEntry.Value >= beta)
                {
                    bestMove = move;
                    return -childEntry.Value;
                }
            }
        }

        if (look >= EvaluationOrderMinLook)
        {
            OrderByEvaluation(list, hashMove, board, player);
        }
        else
        {
            _ordering.Order(list, hashMove, previousMove, player, board);
        }

        int best = -Infinity;
        Bound bound = Bound.Lower;
        foreach (int move in list)
        {
            Board child = Play(board, player, move);
            int score;

            if (look == 0)
            {
                score = -EvaluateLeaf(child, opponent);
            }
            else if (best == -Infinity || beta - alpha == 1)
            {
                score = -Search(child, opponent, look - 1, ply + 1, move, -beta, -alpha, out int reply);
                _ordering.RecordResponse(opponent, move, reply, refuted: score <= alpha);
            }
            else
            {
                // Principal variation search: a later move first only has to show it is not better.
                score = -Search(child, opponent, look - 1, ply + 1, move, -alpha - 1, -alpha, out int reply);
                if (score > alpha && score < beta)
                {
                    score = -Search(child, opponent, look - 1, ply + 1, move, -beta, -alpha, out reply);
                }

                _ordering.RecordResponse(opponent, move, reply, refuted: score <= alpha);
            }

            if (score > best)
            {
                bound = score >= beta ? Bound.Lower : score > alpha ? Bound.Exact : Bound.Upper;
                alpha = Math.Max(alpha, score);
                best = score;
                bestMove = move;
                if (best >= beta)
                {
                    break;
                }
            }
        }

        if (hashPut)
        {
            _midgameTable.Store(own, other, tag, storedLook, bound, best, bestMove);
        }

        return best;
    }

    // Egaroucid's midgame ordering (move_ordering.hpp, depth 0): the hash move, then fewer replies, less potential
    // mobility for the opponent and a better evaluation first. The response killers are not used here: with them first
    // the search was weaker.
    private void OrderByEvaluation(Span<int> list, int hashMove, in Board board, Player player)
    {
        Player opponent = player.Opponent();
        ulong empty = ~(board.Discs(player) | board.Discs(opponent));
        Span<int> keys = stackalloc int[list.Length];
        for (int i = 0; i < list.Length; i++)
        {
            int move = list[i];
            if (move == hashMove)
            {
                keys[i] = int.MaxValue;
                continue;
            }

            Board child = Play(board, player, move);
            ulong childOwn = child.Discs(opponent);
            ulong childOther = child.Discs(player);
            int replies = Endgame.EndgameBits.WeightedMoves(Bitboards.LegalMoves(childOwn, childOther));
            int potential = Endgame.EndgameBits.PotentialMobility(childOther, empty & ~(1UL << move));
            int value = EvaluateLeaf(child, opponent);
            keys[i] = (OrderOffset - replies) * OrderMobility + (OrderOffset - potential) * OrderPotentialMobility
                - value * OrderValue / PatternUnitsPerDisc;
        }

        MoveOrdering.SortDescending(list, keys);
    }

    private bool TryProbCut(in Board board, Player player, int look, int ply, int previousMove, int alpha, int beta, out int cut)
    {
        int depth = look + 1;
        int shallowLook = ProbCutShallowDepth(depth) - 1;
        int error = _probCutErrors![64 - board.EmptyCount, depth];
        int error0 = Math.Max(PatternUnitsPerDisc, error - ProbCutPreCheckOffset);
        int static0 = EvaluateLeaf(board, player);
        cut = 0;
        _inProbCut = true;
        try
        {
            if (static0 >= beta + error0 && beta + error < ProbCutScoreLimit)
            {
                int pcBeta = beta + error;
                if (Search(board, player, shallowLook, ply, previousMove, pcBeta - 1, pcBeta, out _) >= pcBeta)
                {
                    cut = beta;
                    return true;
                }
            }

            if (static0 <= alpha - error0 && alpha - error > -ProbCutScoreLimit)
            {
                int pcAlpha = alpha - error;
                if (Search(board, player, shallowLook, ply, previousMove, pcAlpha, pcAlpha + 1, out _) <= pcAlpha)
                {
                    cut = alpha;
                    return true;
                }
            }
        }
        finally
        {
            _inProbCut = false;
        }

        return false;
    }

    // Egaroucid's shallow depth: about half the depth, with the same parity.
    private static int ProbCutShallowDepth(int depth) => ((depth / 2) & ~1) + (depth & 1);

    // t × σ in 1/100 disc by disc count and depth. σ = deep − shallow search, fitted by the trainer's mpc command on
    // 11,700 test positions searched to depth 13 without selectivity (Plans/improve-stello-strength-findings.md).
    private static int[,] ProbCutErrors(double t)
    {
        var errors = new int[65, 61];
        for (int discs = 0; discs <= 64; discs++)
        {
            double x = discs / 64.0;
            for (int depth = 1; depth <= 60; depth++)
            {
                int shallow = ProbCutShallowDepth(depth);
                double sigma = 1.8871 + 2.9543 * x + 0.0660 * x * x - 0.6749 * shallow + 0.2778 * depth;
                errors[discs, depth] = (int)Math.Ceiling(t * Math.Max(0.5, sigma) * PatternUnitsPerDisc);
            }
        }

        return errors;
    }

    private (int Score, int Move) SolveRoot(in Board board, Player player, int alpha, int beta, ScoreKind kind, int[] rootMoves, ref Best best)
    {
        ulong own = board.Discs(player);
        ulong opponent = board.Discs(player.Opponent());
        int empties = board.EmptyCount;
        int bestScore = -Infinity;
        int bestMove = -1;
        Span<int> improvements = stackalloc int[rootMoves.Length];
        int improved = 0;

        for (int i = 0; i < rootMoves.Length; i++)
        {
            int move = rootMoves[i];
            bool hasSolvedMove = bestMove >= 0 || best.Kind is ScoreKind.WinLossDraw or ScoreKind.Exact;
            _reportDepth = hasSolvedMove ? empties : best.Depth;
            _reportMove = move;
            _reportBestMove = bestMove >= 0 ? bestMove : best.Move;
            _reportScore = bestMove >= 0 ? bestScore : best.Score;
            _reportKind = bestMove >= 0 ? kind : best.Kind;
            ulong flips = Bitboards.Flips(own, opponent, move);
            ulong childOwn = own | flips | (1UL << move);
            ulong childOpponent = opponent & ~flips;
            int score;

            if (i == 0)
            {
                score = -SolveChild(childOpponent, childOwn, -beta, -alpha, empties - 1);
            }
            else
            {
                score = -SolveChild(childOpponent, childOwn, -alpha - 1, -alpha, empties - 1);
                if (score > alpha && score < beta)
                {
                    score = -SolveChild(childOpponent, childOwn, -beta, -alpha, empties - 1);
                }
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestMove = move;
                alpha = Math.Max(alpha, score);
                improvements[improved++] = move;
                if (kind == ScoreKind.Exact || score >= 0 || best.Kind == ScoreKind.None)
                {
                    best = new Best(move, score, kind == ScoreKind.WinLossDraw && score == 0 ? ScoreKind.Exact : kind, empties);
                }
            }

            Report(empties, move, bestMove, bestScore, kind);
            if (bestScore >= beta)
            {
                break;
            }
        }

        MoveToFront(rootMoves, improvements[..improved]);
        return (bestScore, bestMove);
    }

    // Near the end: no move list, no hash table; empty squares in odd regions first (parity).
    private int SolveShallow(ulong own, ulong opponent, int alpha, int beta, int empties, bool passed)
    {
        if ((++_nodes & 1023) == 0)
        {
            CheckAbort();
        }

        ulong empty = ~(own | opponent);
        if (empties == 1)
        {
            return SolveLast(own, opponent, empty);
        }

        if (empties == 2)
        {
            return SolveLast2(own, opponent, alpha, beta, empty, passed);
        }

        ulong odd = 0;
        foreach (ulong quadrant in Quadrants)
        {
            if ((BitOperations.PopCount(empty & quadrant) & 1) != 0)
            {
                odd |= quadrant;
            }
        }

        int best = -Infinity;
        for (int pass = 0; pass < 2; pass++)
        {
            for (ulong squares = empty & (pass == 0 ? odd : ~odd); squares != 0; squares &= squares - 1)
            {
                int square = BitOperations.TrailingZeroCount(squares);
                ulong flips = Bitboards.Flips(own, opponent, square);
                if (flips == 0)
                {
                    continue;
                }

                int score = -SolveShallow(opponent & ~flips, own | flips | (1UL << square), -beta, -alpha, empties - 1, passed: false);
                if (score > best)
                {
                    best = score;
                    if (best > alpha)
                    {
                        alpha = best;
                        if (alpha >= beta)
                        {
                            return best;
                        }
                    }
                }
            }
        }

        if (best > -Infinity)
        {
            return best;
        }

        if (passed)
        {
            _evaluations++;
            return FinalScore(own, opponent);
        }

        return -SolveShallow(opponent, own, -beta, -alpha, empties, passed: true);
    }

    // Two empty squares, without the parity loop; the node itself is counted by the caller.
    private int SolveLast2(ulong own, ulong opponent, int alpha, int beta, ulong empty, bool passed)
    {
        int first = BitOperations.TrailingZeroCount(empty);
        int second = BitOperations.TrailingZeroCount(empty & (empty - 1));
        int best = -Infinity;

        ulong flips = Bitboards.Flips(own, opponent, first);
        if (flips != 0)
        {
            _nodes++;
            best = -SolveLast(opponent & ~flips, own | flips | (1UL << first), 1UL << second);
            if (best >= beta)
            {
                return best;
            }
        }

        flips = Bitboards.Flips(own, opponent, second);
        if (flips != 0)
        {
            _nodes++;
            best = Math.Max(best, -SolveLast(opponent & ~flips, own | flips | (1UL << second), 1UL << first));
        }

        if (best > -Infinity)
        {
            return best;
        }

        if (passed)
        {
            _evaluations++;
            return FinalScore(own, opponent);
        }

        if ((++_nodes & 1023) == 0)
        {
            CheckAbort();
        }

        return -SolveLast2(opponent, own, -beta, -alpha, empty, passed: true);
    }

    private int SolveLast(ulong own, ulong opponent, ulong empty)
    {
        _evaluations++;
        int square = BitOperations.TrailingZeroCount(empty);
        ulong bit = 1UL << square;

        ulong flips = Bitboards.Flips(own, opponent, square);
        if (flips != 0)
        {
            return FinalScore(own | flips | bit, opponent & ~flips);
        }

        flips = Bitboards.Flips(opponent, own, square);
        return flips != 0
            ? FinalScore(own & ~flips, opponent | flips | bit)
            : FinalScore(own, opponent);
    }

    private int EvaluateLeaf(in Board board, Player player)
    {
        _nodes++;
        _evaluations++;
        return PatternScore(board, player);
    }

    // Capped at ±64 discs, the largest possible result.
    private int PatternScore(in Board board, Player player) =>
        Math.Clamp(
            _pattern.Evaluate(board.Discs(player), board.Discs(player.Opponent())) * PatternUnitsPerDisc / _pattern.UnitsPerDisc,
            -64 * PatternUnitsPerDisc,
            64 * PatternUnitsPerDisc);

    // SearchOptions.SoftTimePercent: no new iteration after this share of the hard limit.
    private TimeBudget WithSoftPercent(TimeBudget budget) =>
        _softTimePercent is int percent && budget.HardMs != long.MaxValue
            ? budget with { SoftMs = budget.HardMs * percent / 100 }
            : budget;

    private bool StopIterating(SearchLimits limits, TimeBudget budget, int nextLook) => limits.Mode switch
    {
        TimeControlMode.FixedDepth => nextLook >= limits.Depth,
        TimeControlMode.Solve => false,
        _ => _clock.ElapsedMilliseconds > budget.SoftMs,
    };

    private void CheckAbort()
    {
        if (_progress is not null && _reportMove >= 0 && _clock.ElapsedMilliseconds - _lastHeartbeatMs >= 100)
        {
            Report(_reportDepth, _reportMove, _reportBestMove, _reportScore, _reportKind);
        }

        _cancel.ThrowIfCancellationRequested();
        if (_moveNow.IsCancellationRequested || _clock.ElapsedMilliseconds >= _hardLimitMs)
        {
            throw new SearchAbortedException();
        }
    }

    private void Report(int depth, int move, int bestMove, int score, ScoreKind kind)
    {
        _lastHeartbeatMs = _clock.ElapsedMilliseconds;
        _progress?.Report(new SearchInfo(
            depth,
            new Square(move),
            bestMove >= 0 ? new Square(bestMove) : null,
            score,
            kind,
            _nodes,
            _evaluations,
            _clock.Elapsed));
    }

    private SearchResult Result(Best best) => new(
        best.Move >= 0 ? new Move(new Square(best.Move)) : Move.Pass,
        best.Score,
        best.Kind,
        best.Depth,
        _nodes,
        _evaluations,
        _clock.Elapsed);

    private static Board Play(in Board board, Player player, int move)
    {
        ulong flips = Bitboards.Flips(board.Discs(player), board.Discs(player.Opponent()), move);
        ulong placed = flips | (1UL << move);
        return player == Player.Black
            ? new Board(board.Black | placed, board.White & ~flips)
            : new Board(board.Black & ~flips, board.White | placed);
    }

    private static int FinalScore(ulong own, ulong opponent)
    {
        int ownCount = BitOperations.PopCount(own);
        int opponentCount = BitOperations.PopCount(opponent);
        int empties = 64 - ownCount - opponentCount;
        int difference = ownCount - opponentCount;
        return difference > 0 ? difference + empties : difference < 0 ? difference - empties : 0;
    }

    private static int GameOverScore(ulong own, ulong opponent)
    {
        int difference = FinalScore(own, opponent);
        return difference > 0 ? difference + WinScore
            : difference < 0 ? difference - WinScore
            : 0;
    }

    private static void Fill(Span<int> list, ulong moves)
    {
        for (int i = 0; moves != 0; i++, moves &= moves - 1)
        {
            list[i] = BitOperations.TrailingZeroCount(moves);
        }
    }

    // Each new best move goes to the front, so the last best move is searched first next time (C++: put_in_front).
    private static void MoveToFront(int[] moves, ReadOnlySpan<int> improvements)
    {
        foreach (int move in improvements)
        {
            int index = Array.IndexOf(moves, move);
            Array.Copy(moves, 0, moves, 1, index);
            moves[0] = move;
        }
    }

    private readonly record struct Best(int Move, int Score, ScoreKind Kind, int Depth);

    private sealed class SearchAbortedException : Exception;
}
