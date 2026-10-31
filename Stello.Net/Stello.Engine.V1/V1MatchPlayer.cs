namespace Stello.Engine.V1;

/// <summary>Stello 1's engine with its opening book as a match player (as <see cref="StelloMatchPlayer"/>).</summary>
internal sealed class V1MatchPlayer(int hashBits, OpeningBook? book) : IMatchPlayer
{
    private readonly SearchEngine _engine = new(hashBits);
    private ComputerPlayer? _player;

    public void NewGame()
    {
        _engine.ClearHash();
        _player = new ComputerPlayer(_engine, book, new Random(0));
    }

    public SearchResult ChooseMove(Board board, Player player, SearchLimits limits, CancellationToken cancellationToken) =>
        (_player ?? throw new InvalidOperationException("Call NewGame before the first move."))
            .ChooseMove(board, player, limits, cancellationToken: cancellationToken);
}
