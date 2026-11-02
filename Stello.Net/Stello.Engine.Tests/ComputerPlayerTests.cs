namespace Stello.Engine.Tests;

public class ComputerPlayerTests
{
    private static readonly OpeningBook Book = OpeningBook.Load(BookTestData.BinaryPath);

    [Fact]
    public void ChooseMove_UsesTheBookFirst()
    {
        var player = new ComputerPlayer(new SearchEngine(hashBits: 12), Book, new Random(0));

        SearchResult result = player.ChooseMove(TestGames.Play("d3").Board, Player.White, SearchLimits.FixedDepth(4));

        Assert.Equal(ScoreKind.Book, result.Kind);
        Assert.Equal(new Move(Square.Parse("c5")), result.Move);
    }

    [Fact]
    public void ChooseMove_SearchesOutsideTheBook()
    {
        var player = new ComputerPlayer(new SearchEngine(hashBits: 12), Book, new Random(0));
        (Board board, Player toMove) = TestPositions.RandomPositions(seed: 5, count: 1, plies: 40).Single();

        SearchResult result = player.ChooseMove(board, toMove, SearchLimits.FixedDepth(2));

        Assert.NotEqual(ScoreKind.Book, result.Kind);
        Assert.True(player.BookTracker.ShouldConsult);
    }

    [Fact]
    public void ChooseMove_StopsAskingTheBookAfterThreeMisses()
    {
        var player = new ComputerPlayer(new SearchEngine(hashBits: 12), Book, new Random(0));
        (Board board, Player toMove) = TestPositions.RandomPositions(seed: 5, count: 1, plies: 40).Single();

        for (int i = 0; i < 3; i++)
        {
            player.ChooseMove(board, toMove, SearchLimits.FixedDepth(1));
        }

        Assert.False(player.BookTracker.ShouldConsult);
        Assert.NotEqual(ScoreKind.Book, player.ChooseMove(TestGames.Play("d3").Board, Player.White, SearchLimits.FixedDepth(1)).Kind);
    }

    [Fact]
    public void ChooseMove_WithoutBookSearches()
    {
        var player = new ComputerPlayer(new SearchEngine(hashBits: 12), book: null, new Random(0));

        SearchResult result = player.ChooseMove(Board.Initial, Player.Black, SearchLimits.FixedDepth(2));

        Assert.Equal(ScoreKind.Heuristic, result.Kind);
    }

    [Fact]
    public void ChooseMove_PassesWithoutAskingTheBook()
    {
        var player = new ComputerPlayer(new SearchEngine(hashBits: 12), Book, new Random(0));

        SearchResult result = player.ChooseMove(TestGames.Play(TestGames.BlackMustPass).Board, Player.Black, SearchLimits.FixedDepth(2));

        Assert.True(result.Move.IsPass);
        Assert.True(player.BookTracker.ShouldConsult);
    }

    [Fact]
    public void TryChooseBookMove_IsTheBookStepOfChooseMove()
    {
        // The web app calls it on the page and searches in its worker; the engine here never searches.
        var player = new ComputerPlayer(new SearchEngine(hashBits: 12), Book, new Random(0));
        (Board outside, Player toMove) = TestPositions.RandomPositions(seed: 5, count: 1, plies: 40).Single();

        Assert.True(player.TryChooseBookMove(TestGames.Play("d3").Board, Player.White, SearchLimits.FixedDepth(4), out SearchResult? book));
        Assert.Equal(ScoreKind.Book, book.Kind);
        Assert.Equal(new Move(Square.Parse("c5")), book.Move);
        Assert.False(player.TryChooseBookMove(TestGames.Play("d3").Board, Player.White, SearchLimits.FixedDepth(4, useOpeningBook: false), out _));

        for (int i = 0; i < 3; i++)
        {
            Assert.False(player.TryChooseBookMove(outside, toMove, SearchLimits.FixedDepth(4), out SearchResult? none));
            Assert.Null(none);
        }

        Assert.False(player.BookTracker.ShouldConsult);
        Assert.False(player.TryChooseBookMove(TestGames.Play("d3").Board, Player.White, SearchLimits.FixedDepth(4), out _));
    }
}
