using Stello.Engine.Search;

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

    // After d3 this book has only c5; c3 and e3 are outside it. c5 leads out of the book.
    private static OpeningBook OneMoveBook(int value) => BookTestData.ReadText($"# Stello opening book, format 2\nd3 c5:{value}:H\n");

    private static ComputerPlayer CheckingPlayer(OpeningBook book, BookCheck check) =>
        new(new SearchEngine(12, Search.SearchOptions.Default with { BookCheck = check }), book, new Random(0));

    [Theory]
    [InlineData(true)]
    [InlineData(false)]
    public void ChooseMove_PlaysAMoveOutsideTheBookThatIsClearlyBetter(bool everyPosition)
    {
        ComputerPlayer player = CheckingPlayer(OneMoveBook(-3000), everyPosition ? BookCheck.All : BookCheck.Exit);

        SearchResult result = player.ChooseMove(TestGames.Play("d3").Board, Player.White, SearchLimits.FixedDepth(4));

        Assert.Equal(ScoreKind.Heuristic, result.Kind);
        Assert.Contains(result.Move.Square!.Value, new[] { Square.Parse("c3"), Square.Parse("e3") });
    }

    [Fact]
    public void ChooseMove_KeepsTheBookMoveWhenTheOthersAreNotBetter()
    {
        ComputerPlayer player = CheckingPlayer(OneMoveBook(3000), BookCheck.Exit);

        SearchResult result = player.ChooseMove(TestGames.Play("d3").Board, Player.White, SearchLimits.FixedDepth(4));

        Assert.Equal(ScoreKind.Book, result.Kind);
        Assert.Equal(new Move(Square.Parse("c5")), result.Move);
        Assert.True(result.Nodes > 0);
    }

    [Fact]
    public void ChooseMove_ChecksOnlyWhenLeavingTheBookInExitMode()
    {
        // d3 c5 is in the book, so the book move c5 does not leave it.
        OpeningBook book = BookTestData.ReadText("# Stello opening book, format 2\nd3 c5:-3000:B\nd3c5 d6:3000:H\n");

        SearchResult exit = CheckingPlayer(book, BookCheck.Exit).ChooseMove(TestGames.Play("d3").Board, Player.White, SearchLimits.FixedDepth(4));
        SearchResult all = CheckingPlayer(book, BookCheck.All).ChooseMove(TestGames.Play("d3").Board, Player.White, SearchLimits.FixedDepth(4));

        Assert.Equal(ScoreKind.Book, exit.Kind);
        Assert.Equal(0, exit.Nodes);
        Assert.Equal(ScoreKind.Heuristic, all.Kind);
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
}
