using Stello.Engine.Evaluation;

namespace Stello.Engine;

public enum GameResult
{
    BlackWins,
    WhiteWins,
    Draw,
}

public enum BookLearningStage
{
    EvaluatingPositions,
    PlayingGame,
}

public sealed record BookLearningProgress(BookLearningStage Stage, int PositionsEvaluated, int GamesPlayed, int NodeCount);

/// <summary>
/// Book learning from Book.cpp: adding games to the book (Flet spil: convert_game, mmgame), searching the
/// book's leaves and the best move not yet in the book (Minmaxlib: calc_lib, minmaxlib), backing the values up
/// the tree (minmax_lib, mmlib), sorting it best first (sort_lib), and self-play (Lær spil: selfplay, splay).
/// Not thread-safe; nothing else may use the book while it learns.
/// </summary>
public sealed class BookLearner
{
    // C++ mmgame: a move in an added game is a sure win (or loss) for the player who makes it.
    private const short WinValue = 32665;

    // C++ mmgame stops at depth 56.
    private const int MaxGameDepth = 56;

    // C++: minmax_lib is called 10 times so values also travel through transpositions.
    private const int MinimaxRounds = 10;

    // C++: the book is saved every 10 searched positions (libmoves % 10).
    private const int CheckpointInterval = 10;

    private readonly OpeningBook _book;
    private readonly SearchEngine _engine;
    private readonly SearchLimits _limits;
    private readonly Random _random;
    private readonly Action<OpeningBook>? _checkpoint;
    private IProgress<BookLearningProgress>? _progress;
    private BookLearningStage _stage;
    private CancellationToken _cancel;

    /// <param name="limits">How long each position is searched (C++: 2 minutes, level LIBLEVEL 11).</param>
    /// <param name="checkpoint">Called now and then to save the book, so a long run is not lost.</param>
    public BookLearner(OpeningBook book, SearchEngine engine, SearchLimits limits, Random random, Action<OpeningBook>? checkpoint = null)
    {
        _book = book;
        _engine = engine;
        _limits = limits;
        _random = random;
        _checkpoint = checkpoint;
    }

    public int PositionsEvaluated { get; private set; }

    public int GamesPlayed { get; private set; }

    /// <summary>Adds the moves of a game to the book (C++: convert_game + mmgame); existing lines are kept.</summary>
    /// <exception cref="ArgumentException">The moves are not a legal game from the start position.</exception>
    public void AddGame(IReadOnlyList<Move> moves, GameResult result)
    {
        if (moves.Count < 2 || moves[0].Square is not { } first)
        {
            return;
        }

        Board board = Apply(Board.Initial, Player.Black, moves[0]);
        Player player = Player.White;
        List<BookNode> replies = _book.Root;
        OpeningBook.Symmetry symmetry = OpeningBook.FirstMoveSymmetry(first);

        for (int ply = 1; ply < moves.Count && ply <= MaxGameDepth; ply++)
        {
            Move move = moves[ply];
            Board next = Apply(board, player, move);
            Player nextPlayer = player.Opponent();

            // Continue where the book already has the position, also when it was reached by another move order.
            if (_book.TryFindReplies(next, nextPlayer, out List<BookNode> known, out OpeningBook.Symmetry knownSymmetry))
            {
                replies = known;
                symmetry = knownSymmetry;
            }
            else
            {
                short legacy = move.Square is { } square ? (short)OpeningBook.Transform(square, symmetry).ToLegacy() : (short)0;
                BookNode? node = replies.Find(n => n.Move == legacy);
                if (node is null)
                {
                    node = new BookNode(legacy, ValueFor(player, result), BookFlags.None);
                    replies.Add(node);
                }

                replies = node.Children;
            }

            board = next;
            player = nextPlayer;
        }

        _book.Rebuild();
    }

    /// <summary>
    /// Searches every leaf that has not been searched yet, and adds the best move that is not in the book to
    /// every position whose replies have not been searched (C++: calc_lib/minmaxlib).
    /// </summary>
    public void EvaluatePositions(IProgress<BookLearningProgress>? progress = null, CancellationToken cancellationToken = default)
    {
        _progress = progress;
        _cancel = cancellationToken;
        _stage = BookLearningStage.EvaluatingPositions;
        try
        {
            EvaluateReplies(_book.Root, StartBoard, Player.White);
        }
        finally
        {
            _book.Rebuild();
        }
    }

    /// <summary>Backs the values up the tree and sorts every list of replies best first (C++: minmax_lib, sort_lib).</summary>
    public void Minimax()
    {
        for (int round = 0; round < MinimaxRounds; round++)
        {
            BackUp(_book.Root, StartBoard, Player.White);
        }

        Sort(_book.Root);
        _book.Rebuild();
    }

    /// <summary>The engine plays a game against itself with the book, until the endgame is solved (C++: splay).</summary>
    public (IReadOnlyList<Move> Moves, GameResult Result) PlayGame(CancellationToken cancellationToken = default)
    {
        var computer = new ComputerPlayer(_engine, _book, _random);
        var game = new Game();

        while (!game.IsGameOver)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (game.MustPass)
            {
                game.Pass();
                continue;
            }

            Player mover = game.ToMove;
            SearchResult result = computer.ChooseMove(game.Board, mover, _limits, cancellationToken: cancellationToken);
            game.Play(result.Move);

            if (result.Kind == ScoreKind.Exact)
            {
                return (game.PlayedMoves.ToList(), ResultFor(mover, result.Score));
            }
        }

        return (game.PlayedMoves.ToList(), game.Winner switch
        {
            Player.Black => GameResult.BlackWins,
            Player.White => GameResult.WhiteWins,
            _ => GameResult.Draw,
        });
    }

    /// <summary>
    /// Evaluates the book, then plays games against itself and adds them, until cancelled (C++: selfplay).
    /// </summary>
    /// <param name="gamePlayed">Called after each game, e.g. to write the SELFPLAY log.</param>
    /// <exception cref="OperationCanceledException">Always, when <paramref name="cancellationToken"/> is cancelled.</exception>
    public void SelfPlay(
        IProgress<BookLearningProgress>? progress,
        CancellationToken cancellationToken,
        Action<int, IReadOnlyList<Move>>? gamePlayed = null)
    {
        Learn(progress, cancellationToken);

        while (true)
        {
            _stage = BookLearningStage.PlayingGame;
            Report();
            (IReadOnlyList<Move> moves, GameResult result) = PlayGame(cancellationToken);
            AddGame(moves, result);
            GamesPlayed++;
            _checkpoint?.Invoke(_book);
            gamePlayed?.Invoke(GamesPlayed, moves);

            Learn(progress, cancellationToken);
        }
    }

    private static Board StartBoard { get; } = Board.Initial.Play(Player.Black, Square.Parse("d3"));

    // C++ selfplay: calc_lib twice, minmax_lib ten times, sort_lib, Put_book.
    private void Learn(IProgress<BookLearningProgress>? progress, CancellationToken cancellationToken)
    {
        EvaluatePositions(progress, cancellationToken);
        EvaluatePositions(progress, cancellationToken);
        Minimax();
        _checkpoint?.Invoke(_book);
    }

    // C++: minmaxlib. Returns the value of the position for the player to move.
    private int EvaluateReplies(List<BookNode> replies, Board board, Player player)
    {
        Player opponent = player.Opponent();

        if (replies.Count > 0 && replies[0].Move == 0)
        {
            BookNode pass = replies[0];
            if (pass.Children.Count > 0)
            {
                pass.Value = Clamp(-EvaluateReplies(pass.Children, board, opponent));
                pass.Flag = BookFlags.None;
            }
            else if (!pass.Flag.HasFlag(BookFlags.Calculated))
            {
                (int value, BookFlags flags) = SearchValue(board, opponent);
                pass.Value = Clamp(-value);
                pass.Flag = flags;
            }

            return pass.Value;
        }

        bool mustAdd = !replies.Any(n => n.Flag.HasFlag(BookFlags.Calculated));
        int best = -Infinity;

        for (int i = 0; i < replies.Count;)
        {
            BookNode node = replies[i];
            if (!IsLegal(board, player, node.Move))
            {
                replies.RemoveAt(i);
                continue;
            }

            Board child = board.Play(player, Square.FromLegacy(node.Move));
            int score;
            if (node.Children.Count > 0)
            {
                score = -EvaluateReplies(node.Children, child, opponent);
                node.Flag = BookFlags.None;
            }
            else if (node.Flag.HasFlag(BookFlags.Calculated))
            {
                score = node.Value;
            }
            else if (child.HasLegalMove(opponent) && _book.TryGetMove(child, opponent, _random, out BookMove known))
            {
                // Transposition into another line of the book.
                score = -known.Value;
                node.Flag = BookFlags.None;
            }
            else
            {
                (int value, BookFlags flags) = SearchValue(child, opponent);
                score = -value;
                node.Flag = flags;
            }

            node.Value = Clamp(score);
            best = Math.Max(best, node.Value);
            i++;
        }

        if (mustAdd)
        {
            ulong inBook = replies.Aggregate(0UL, (mask, n) => mask | Square.FromLegacy(n.Move).Bit);
            ulong others = board.LegalMoves(player) & ~inBook;
            if (others != 0)
            {
                (int value, BookFlags flags, Square move) = Search(board, player, others);
                replies.Add(new BookNode((short)move.ToLegacy(), Clamp(value), flags));
                best = Math.Max(best, value);
            }
        }

        return best;
    }

    // C++: mmlib. Returns the value of the position for the player to move.
    private int BackUp(List<BookNode> replies, Board board, Player player)
    {
        Player opponent = player.Opponent();

        if (replies.Count > 0 && replies[0].Move == 0)
        {
            BookNode pass = replies[0];
            if (pass.Children.Count > 0)
            {
                pass.Value = Clamp(-BackUp(pass.Children, board, opponent));
            }
            else if (board.HasLegalMove(opponent) && _book.TryGetMove(board, opponent, _random, out BookMove known))
            {
                pass.Value = Clamp(-known.Value);
            }

            return pass.Value;
        }

        int best = -Infinity;
        for (int i = 0; i < replies.Count;)
        {
            BookNode node = replies[i];
            if (!IsLegal(board, player, node.Move))
            {
                replies.RemoveAt(i);
                continue;
            }

            Board child = board.Play(player, Square.FromLegacy(node.Move));
            if (node.Children.Count > 0)
            {
                node.Value = Clamp(-BackUp(node.Children, child, opponent));
            }
            else if (child.HasLegalMove(opponent) && _book.TryGetMove(child, opponent, _random, out BookMove known))
            {
                node.Value = Clamp(-known.Value);
            }

            best = Math.Max(best, node.Value);
            i++;
        }

        return best;
    }

    // C++: sort_lib. Stable, best value first.
    private static void Sort(List<BookNode> replies)
    {
        List<BookNode> sorted = replies.OrderByDescending(n => n.Value).ToList();
        replies.Clear();
        replies.AddRange(sorted);
        foreach (BookNode node in replies)
        {
            Sort(node.Children);
        }
    }

    // C++: getvalue. The value of the position for the player to move, from a search.
    private (int Value, BookFlags Flags) SearchValue(Board board, Player player)
    {
        ulong moves = board.LegalMoves(player);
        if (moves != 0)
        {
            (int value, BookFlags flags, _) = Search(board, player, moves);
            return (value, flags);
        }

        Player opponent = player.Opponent();
        if (!board.HasLegalMove(opponent))
        {
            return (GameOverValue(board, player), BookFlags.Calculated | BookFlags.Exact);
        }

        (int passValue, BookFlags passFlags) = SearchValue(board, opponent);
        return (-passValue, passFlags);
    }

    private (int Value, BookFlags Flags, Square Move) Search(Board board, Player player, ulong moves)
    {
        SearchResult result = _engine.Search(board, player, _limits, cancellationToken: _cancel, onlyMoves: moves);

        PositionsEvaluated++;
        Report();
        if (PositionsEvaluated % CheckpointInterval == 0)
        {
            _checkpoint?.Invoke(_book);
        }

        // Solved positions are stored as wins/losses beyond the evaluation range, as in C++.
        int value = result.Kind is ScoreKind.Exact or ScoreKind.WinLossDraw
            ? result.Score > 0 ? Evaluator.WinScore + result.Score
                : result.Score < 0 ? result.Score - Evaluator.WinScore
                : 0
            : result.Score;

        BookFlags flags = BookFlags.Calculated | result.Kind switch
        {
            ScoreKind.Exact => BookFlags.Exact,
            ScoreKind.WinLossDraw => BookFlags.Inexact,
            _ => BookFlags.None,
        };

        return (value, flags, result.Move.Square!.Value);
    }

    private void Report() =>
        _progress?.Report(new BookLearningProgress(_stage, PositionsEvaluated, GamesPlayed, _book.NodeCount));

    private const int Infinity = 32767;

    private static short Clamp(int value) => (short)Math.Clamp(value, -Infinity, Infinity);

    private static bool IsLegal(Board board, Player player, short legacy) =>
        OpeningBook.IsSquare(legacy) && board.IsLegal(player, Square.FromLegacy(legacy));

    private static short ValueFor(Player player, GameResult result) => result switch
    {
        GameResult.Draw => 0,
        GameResult.BlackWins => player == Player.Black ? WinValue : (short)-WinValue,
        _ => player == Player.White ? WinValue : (short)-WinValue,
    };

    private static GameResult ResultFor(Player mover, int score) =>
        score == 0 ? GameResult.Draw
        : (score > 0) == (mover == Player.Black) ? GameResult.BlackWins
        : GameResult.WhiteWins;

    private static int GameOverValue(Board board, Player player)
    {
        int difference = board.Count(player) - board.Count(player.Opponent());
        int empties = board.EmptyCount;
        return difference > 0 ? Evaluator.WinScore + difference + empties
            : difference < 0 ? difference - empties - Evaluator.WinScore
            : 0;
    }

    private static Board Apply(Board board, Player player, Move move)
    {
        if (move.Square is { } square)
        {
            return board.IsLegal(player, square)
                ? board.Play(player, square)
                : throw new ArgumentException($"{square} is not a legal move for {player}.", nameof(move));
        }

        return !board.HasLegalMove(player)
            ? board
            : throw new ArgumentException($"{player} has a legal move and cannot pass.", nameof(move));
    }
}
