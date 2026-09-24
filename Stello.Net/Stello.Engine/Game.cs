namespace Stello.Engine;

/// <summary>
/// A game from the initial position with its move history. Moves after the current position are kept
/// until a new move is played, so undone moves can be redone.
/// </summary>
public sealed class Game
{
    private readonly List<Move> _moves = [];
    private readonly List<Position> _positions = [new(Board.Initial, Player.Black)];

    public Game(Player human = Player.Black)
    {
        Human = human;
    }

    public Player Human { get; private set; }

    public Player Computer => Human.Opponent();

    /// <summary>Number of moves played up to the current position (C++: playnm).</summary>
    public int Ply { get; private set; }

    /// <summary>All moves in the history, including moves after the current position (C++: game.moves/sidste).</summary>
    public IReadOnlyList<Move> Moves => _moves;

    public IEnumerable<Move> PlayedMoves => _moves.Take(Ply);

    public Board Board => _positions[Ply].Board;

    public Player ToMove => _positions[Ply].ToMove;

    public Move? LastMove => Ply > 0 ? _moves[Ply - 1] : null;

    public bool IsHumanToMove => ToMove == Human;

    public bool IsGameOver => Board.IsGameOver;

    public bool MustPass => !IsGameOver && !Board.HasLegalMove(ToMove);

    public bool CanUndo => Ply > 0;

    public bool CanRedo => Ply < _moves.Count;

    /// <summary>The player with most discs when the game is over; null while the game is running or when it is a draw.</summary>
    public Player? Winner
    {
        get
        {
            if (!IsGameOver)
            {
                return null;
            }

            int difference = Board.Count(Player.Black) - Board.Count(Player.White);
            return difference > 0 ? Player.Black : difference < 0 ? Player.White : null;
        }
    }

    public void Play(Square square)
    {
        if (IsGameOver)
        {
            throw new InvalidOperationException("The game is over.");
        }

        Board next = Board.Play(ToMove, square);
        Append(new Move(square), next);
    }

    public void Pass()
    {
        if (!MustPass)
        {
            throw new InvalidOperationException(IsGameOver
                ? "The game is over."
                : $"{ToMove} has a legal move and cannot pass.");
        }

        Append(Move.Pass, Board);
    }

    public void Play(Move move)
    {
        if (move.Square is { } square)
        {
            Play(square);
        }
        else
        {
            Pass();
        }
    }

    public void Undo()
    {
        if (!CanUndo)
        {
            throw new InvalidOperationException("There is no move to undo.");
        }

        Ply--;
    }

    public void Redo()
    {
        if (!CanRedo)
        {
            throw new InvalidOperationException("There is no move to redo.");
        }

        Ply++;
    }

    public void NewGame()
    {
        _moves.Clear();
        _positions.RemoveRange(1, _positions.Count - 1);
        Ply = 0;
    }

    // C++: OnSkiftSide.
    public void SwitchSides() => Human = Human.Opponent();

    private void Append(Move move, Board next)
    {
        Player mover = ToMove;
        _moves.RemoveRange(Ply, _moves.Count - Ply);
        _positions.RemoveRange(Ply + 1, _positions.Count - Ply - 1);

        _moves.Add(move);
        _positions.Add(new Position(next, mover.Opponent()));
        Ply++;
    }

    private readonly record struct Position(Board Board, Player ToMove);
}
