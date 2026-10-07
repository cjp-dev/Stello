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

    /// <summary>Adds the moves of a game to the book (C++: convert_game + mmgame); existing moves are kept.</summary>
    /// <exception cref="ArgumentException">The moves are not a legal game from the start position.</exception>
    public void AddGame(IReadOnlyList<Move> moves, GameResult result)
    {
        if (moves.Count < 2 || moves[0].IsPass)
        {
            return;
        }

        Board board = Apply(Board.Initial, Player.Black, moves[0]);
        Player player = Player.White;

        for (int ply = 1; ply < moves.Count && ply <= MaxGameDepth; ply++)
        {
            Move move = moves[ply];
            Board next = Apply(board, player, move);

            // A position reached by another move order or another first move is the same book position.
            List<BookEntry> replies = _book.GetOrAddReplies(board, player, out OpeningBook.Symmetry symmetry);
            Move canonical = OpeningBook.Transform(move, symmetry);
            if (!replies.Exists(entry => entry.Move == canonical))
            {
                replies.Add(new BookEntry(canonical, ValueFor(player, result), BookOrigin.Unknown));
            }

            board = next;
            player = player.Opponent();
        }
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
        if (_book.Contains(OpeningBook.RootBoard, OpeningBook.RootPlayer))
        {
            EvaluateReplies(OpeningBook.RootBoard, OpeningBook.RootPlayer, []);
        }
    }

    /// <summary>Backs the values up the tree and sorts every list of replies best first (C++: minmax_lib, sort_lib).</summary>
    public void Minimax() => BookMinimax.Run(_book);

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

    // C++ selfplay: calc_lib twice, minmax_lib ten times, sort_lib, Put_book.
    private void Learn(IProgress<BookLearningProgress>? progress, CancellationToken cancellationToken)
    {
        EvaluatePositions(progress, cancellationToken);
        EvaluatePositions(progress, cancellationToken);
        Minimax();
        _checkpoint?.Invoke(_book);
    }

    // C++: minmaxlib. Returns the value of the position for the player to move. A position reached by several
    // move orders is evaluated once.
    private int EvaluateReplies(Board board, Player player, Dictionary<BookKey, int> evaluated)
    {
        (BookKey key, OpeningBook.Symmetry symmetry) = OpeningBook.Canonical(board, player);
        if (evaluated.TryGetValue(key, out int known))
        {
            return known;
        }

        _book.TryGetReplies(key, out List<BookEntry>? replies);
        bool mustAdd = !replies!.Exists(entry => entry.IsSearched);
        int best = -short.MaxValue;
        ulong inBook = 0;

        foreach (BookEntry entry in replies)
        {
            Move move = OpeningBook.Transform(entry.Move, symmetry);
            inBook |= move.Square?.Bit ?? 0;
            (Board child, Player opponent) = OpeningBook.Play(board, player, move);
            if (_book.Contains(child, opponent))
            {
                entry.Set(BookMinimax.Clamp(-EvaluateReplies(child, opponent, evaluated)), BookOrigin.BackedUp);
            }
            else if (!entry.IsSearched)
            {
                (int value, BookOrigin origin, BookEffort effort) =
                    BookSearch.PositionValue(_engine, child, opponent, _limits, _cancel, Searched);
                entry.Set(BookMinimax.Clamp(-value), origin, effort);
            }

            best = Math.Max(best, entry.Value);
        }

        // Dropout expansion: the best move that is not in the book, so the book knows whether its moves are best.
        ulong others = board.LegalMoves(player) & ~inBook;
        if (mustAdd && others != 0)
        {
            (int value, BookOrigin origin, BookEffort effort, Square square) =
                BookSearch.Search(_engine, board, player, others, _limits, _cancel);
            Searched();
            replies.Add(new BookEntry(OpeningBook.Transform(new Move(square), symmetry), BookMinimax.Clamp(value), origin, effort));
            best = Math.Max(best, value);
        }

        evaluated[key] = best;
        return best;
    }

    private void Searched()
    {
        PositionsEvaluated++;
        Report();
        if (PositionsEvaluated % CheckpointInterval == 0)
        {
            _checkpoint?.Invoke(_book);
        }
    }

    private void Report() =>
        _progress?.Report(new BookLearningProgress(_stage, PositionsEvaluated, GamesPlayed, _book.NodeCount));

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
