using System.Diagnostics;
using System.Numerics;
using Stello.Engine.Evaluation;
using Stello.Engine.Search;

namespace Stello.Engine;

/// <summary>
/// Alpha-beta search ported from Minmax.cpp and Kontrol.cpp: iterative deepening with a hash table, response
/// killers, selective search, and an exact endgame solver. An instance runs one search at a time.
/// </summary>
public sealed class SearchEngine
{
    private const int Infinity = 32767;
    private const int RootWindow = 32664;

    // C++: the endgame solver takes over when (allway - varlook) <= 7.
    private const int EndgameDistance = 7;

    // C++: SELEXT with D1, D2, DP and E_BOUND.
    private const int SelectiveLook1 = 7;
    private const int SelectiveLook2 = 5;
    private const int SelectiveShallowLook = 3;
    private const int SelectiveMargin = 50;
    private const int SelectiveScoreLimit = 32000;

    private const int EndgameHashMinEmpties = 7;
    private const int FastestFirstMinEmpties = 7;
    private const int EvaluationOrderMinEmpties = 18;

    // With this few empty squares the solver tries the empty squares directly instead of generating moves.
    private const int ShallowEmpties = 6;

    // Quadrants a1-d4, e1-h4, a5-d8, e5-h8; squares in regions with an odd number of empties are tried first.
    private static readonly ulong[] Quadrants = [0x0000_0000_0F0F_0F0FUL, 0x0000_0000_F0F0_F0F0UL, 0x0F0F_0F0F_0000_0000UL, 0xF0F0_F0F0_0000_0000UL];

    private const ulong Corners = 0x8100_0000_0000_0081UL;

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

    /// <param name="hashBits">Each of the two hash tables gets 2^hashBits entries (C++: HASHSIZE 19).</param>
    public SearchEngine(int hashBits = 19)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(hashBits, 10);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(hashBits, 26);
        _midgameTable = new TranspositionTable(hashBits);
        _endgameTable = new TranspositionTable(hashBits);
    }

    public void ClearHash()
    {
        _midgameTable.Clear();
        _endgameTable.Clear();
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
        _nodes = 0;
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
        TimeBudget budget = TimeControl.ForMidgame(limits, empties);
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

                    if (StopIterating(limits, budget, look + 1))
                    {
                        return Result(best);
                    }
                }
            }

            budget = TimeControl.ForEndgame(limits, budget, _clock.ElapsedMilliseconds);
            _hardLimitMs = budget.HardMs;

            // Win/loss/draw first with a null window around 0; it is much faster than the exact score.
            (int wld, int wldMove) = SolveRoot(board, player, -1, 1, ScoreKind.WinLossDraw, rootMoves);
            bool keepMidgameMove = wld < 0 && best.Kind == ScoreKind.Heuristic;
            best = new Best(keepMidgameMove ? best.Move : wldMove, wld, wld == 0 ? ScoreKind.Exact : ScoreKind.WinLossDraw, empties);

            if (wld == 0 || StopIterating(limits, budget, 0))
            {
                return Result(best);
            }

            (int alpha, int beta) = wld > 0 ? (wld - 1, 65) : (-65, wld + 1);
            (int exact, int exactMove) = SolveRoot(board, player, alpha, beta, ScoreKind.Exact, rootMoves);
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
            Board child = Play(board, player, move);
            int score;

            if (look == 0 && !Evaluator.IsDangerous(child, player, move))
            {
                score = -EvaluateLeaf(child, opponent, -beta, -alpha, rootMoves.Length);
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

        // Selective search: a shallow null-window search decides if the position is clearly outside the window.
        if (look is SelectiveLook1 or SelectiveLook2)
        {
            if (beta is < SelectiveScoreLimit and > -SelectiveScoreLimit)
            {
                int threshold = beta + Math.Abs(beta) * 5 / 10 + SelectiveMargin;
                if (Search(board, player, SelectiveShallowLook, ply, previousMove, threshold - 1, threshold, out bestMove) >= threshold)
                {
                    return beta;
                }
            }

            if (alpha is < SelectiveScoreLimit and > -SelectiveScoreLimit)
            {
                int threshold = alpha - Math.Abs(alpha) * 5 / 10 - SelectiveMargin;
                if (Search(board, player, SelectiveShallowLook, ply, previousMove, threshold, threshold + 1, out bestMove) <= threshold)
                {
                    return alpha;
                }
            }
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

        int storedLook = look;
        int moveCount = BitOperations.PopCount(moves);

        // A single reply at the horizon is searched one ply deeper.
        if (moveCount == 1 && look == 0)
        {
            look = 1;
        }

        Span<int> list = stackalloc int[moveCount];
        Fill(list, moves);
        _ordering.Order(list, hashMove, previousMove, player, board);

        int best = -Infinity;
        Bound bound = Bound.Lower;
        foreach (int move in list)
        {
            Board child = Play(board, player, move);
            int score;

            if (look == 0 && !Evaluator.IsDangerous(child, player, move))
            {
                score = -EvaluateLeaf(child, opponent, -beta, -alpha, moveCount);
            }
            else
            {
                score = -Search(child, opponent, look > 0 ? look - 1 : 0, ply + 1, move, -beta, -alpha, out int reply);
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

    private (int Score, int Move) SolveRoot(in Board board, Player player, int alpha, int beta, ScoreKind kind, int[] rootMoves)
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
            ulong flips = Bitboards.Flips(own, opponent, move);
            ulong childOwn = own | flips | (1UL << move);
            ulong childOpponent = opponent & ~flips;
            int score;

            if (i == 0)
            {
                score = -Solve(childOpponent, childOwn, -beta, -alpha, empties - 1);
            }
            else
            {
                score = -Solve(childOpponent, childOwn, -alpha - 1, -alpha, empties - 1);
                if (score > alpha && score < beta)
                {
                    score = -Solve(childOpponent, childOwn, -beta, -alpha, empties - 1);
                }
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestMove = move;
                alpha = Math.Max(alpha, score);
                improvements[improved++] = move;
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

    // C++: slutmax1/slutmax3. Scores are final disc differences; empty squares go to the winner.
    private int Solve(ulong own, ulong opponent, int alpha, int beta, int empties)
    {
        if (empties <= ShallowEmpties)
        {
            return SolveShallow(own, opponent, alpha, beta, empties, passed: false);
        }

        if ((++_nodes & 1023) == 0)
        {
            CheckAbort();
        }

        ulong moves = Bitboards.LegalMoves(own, opponent);
        if (moves == 0)
        {
            if (Bitboards.LegalMoves(opponent, own) == 0)
            {
                _evaluations++;
                return FinalScore(own, opponent);
            }

            return -Solve(opponent, own, -beta, -alpha, empties);
        }

        int hashMove = -1;
        bool useHash = empties >= EndgameHashMinEmpties;
        if (useHash && _endgameTable.TryGet(own, opponent, 0, out TranspositionTable.Entry entry))
        {
            if (entry.Bound == Bound.Exact)
            {
                return entry.Value;
            }

            if (entry.Bound == Bound.Lower)
            {
                alpha = Math.Max(alpha, entry.Value);
            }
            else if (entry.Bound == Bound.Upper)
            {
                beta = Math.Min(beta, entry.Value);
            }

            if (alpha >= beta)
            {
                return entry.Value;
            }

            hashMove = entry.Move;
        }

        int alphaOriginal = alpha;
        int count = BitOperations.PopCount(moves);
        Span<int> list = stackalloc int[count];
        Fill(list, moves);
        if (count > 1)
        {
            Span<int> keys = stackalloc int[count];
            for (int i = 0; i < count; i++)
            {
                int square = list[i];
                if (square == hashMove)
                {
                    keys[i] = int.MaxValue;
                }
                else if (empties >= EvaluationOrderMinEmpties)
                {
                    // Far from the end the evaluation orders better; colours do not matter for the solver.
                    ulong flips = Bitboards.Flips(own, opponent, square);
                    var child = new Board(opponent & ~flips, own | flips | (1UL << square));
                    keys[i] = -Evaluator.Evaluate(child, Player.Black, -Infinity, Infinity, count);
                }
                else if (empties >= FastestFirstMinEmpties)
                {
                    // Fastest first: few replies for the opponent (corners count double), then the square value.
                    ulong flips = Bitboards.Flips(own, opponent, square);
                    ulong replies = Bitboards.LegalMoves(opponent & ~flips, own | flips | (1UL << square));
                    int mobility = BitOperations.PopCount(replies) + BitOperations.PopCount(replies & Corners);
                    keys[i] = -mobility * 256 + MoveOrdering.BaseScore(square);
                }
                else
                {
                    keys[i] = MoveOrdering.BaseScore(square);
                }
            }

            MoveOrdering.SortDescending(list, keys);
        }

        int best = -Infinity;
        int bestMove = -1;
        foreach (int move in list)
        {
            ulong flips = Bitboards.Flips(own, opponent, move);
            ulong childOwn = opponent & ~flips;
            ulong childOpponent = own | flips | (1UL << move);
            int score;

            // Principal variation search: later moves only have to prove that they are not better.
            if (best == -Infinity || beta - alpha == 1)
            {
                score = -Solve(childOwn, childOpponent, -beta, -alpha, empties - 1);
            }
            else
            {
                score = -Solve(childOwn, childOpponent, -alpha - 1, -alpha, empties - 1);
                if (score > alpha && score < beta)
                {
                    score = -Solve(childOwn, childOpponent, -beta, -score, empties - 1);
                }
            }

            if (score > best)
            {
                best = score;
                bestMove = move;
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

        if (useHash)
        {
            Bound bound = best >= beta ? Bound.Lower : best > alphaOriginal ? Bound.Exact : Bound.Upper;
            _endgameTable.Store(own, opponent, 0, empties, bound, best, bestMove);
        }

        return best;
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

    private int EvaluateLeaf(in Board board, Player player, int alpha, int beta, int opponentMobility)
    {
        _nodes++;
        _evaluations++;
        return Evaluator.Evaluate(board, player, alpha, beta, opponentMobility);
    }

    private bool StopIterating(SearchLimits limits, TimeBudget budget, int nextLook) => limits.Mode switch
    {
        TimeControlMode.FixedDepth => nextLook >= limits.Depth,
        TimeControlMode.Solve => false,
        _ => _clock.ElapsedMilliseconds > budget.SoftMs,
    };

    private void CheckAbort()
    {
        _cancel.ThrowIfCancellationRequested();
        if (_moveNow.IsCancellationRequested || _clock.ElapsedMilliseconds >= _hardLimitMs)
        {
            throw new SearchAbortedException();
        }
    }

    private void Report(int depth, int move, int bestMove, int score, ScoreKind kind) =>
        _progress?.Report(new SearchInfo(
            depth,
            new Square(move),
            bestMove >= 0 ? new Square(bestMove) : null,
            score,
            kind,
            _nodes,
            _evaluations,
            _clock.Elapsed));

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
        return difference > 0 ? difference + Evaluator.WinScore
            : difference < 0 ? difference - Evaluator.WinScore
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
