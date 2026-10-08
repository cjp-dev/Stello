namespace Stello.Engine.Tests;

public class OpeningBookTests
{
    private const string Header = BookTextFormat.FirstLine + "\n";

    [Fact]
    public void TryGetMove_ChoosesBlacksFirstMoveAtRandom()
    {
        var moves = new HashSet<string>();
        for (int seed = 0; seed < 100; seed++)
        {
            Assert.True(BookTestData.Master.TryGetMove(Board.Initial, Player.Black, new Random(seed), out BookMove move));
            moves.Add(move.Square.ToString());
        }

        Assert.Equal(["c4", "d3", "e6", "f5"], moves.Order());
    }

    [Theory]
    [InlineData("d3", "c5")]
    [InlineData("c4", "e3")]
    [InlineData("f5", "d6")]
    [InlineData("e6", "f4")]
    public void TryGetMove_FindsTheSameReplyForSymmetricFirstMoves(string first, string reply)
    {
        Board board = TestGames.Play(first).Board;

        Assert.True(BookTestData.Master.TryGetMove(board, Player.White, new Random(0), out BookMove move));

        Assert.Equal(Square.Parse(reply), move.Square);
        Assert.Equal(-10, move.Value);
    }

    [Fact]
    public void TryGetMove_GivesTheSameLegalMoveInEveryBookPositionAndItsSymmetries()
    {
        OpeningBook book = BookTestData.Master;
        int positions = 0;

        foreach (BookLine line in BookTextFormat.Lines(book).Where(l => l.Board.HasLegalMove(l.Player)))
        {
            positions++;
            Assert.True(book.TryGetMove(line.Board, line.Player, new Random(0), out BookMove move));
            Assert.True(line.Board.IsLegal(line.Player, move.Square));
            BookKey after = OpeningBook.Canonical(line.Board.Play(line.Player, move.Square), line.Player.Opponent()).Key;

            // In a symmetric position the mirrored move can be another, equivalent move.
            foreach (OpeningBook.Symmetry symmetry in Enum.GetValues<OpeningBook.Symmetry>())
            {
                Board mirrored = OpeningBook.Transform(line.Board, symmetry);
                Assert.True(book.TryGetMove(mirrored, line.Player, new Random(0), out BookMove reply));
                Assert.Equal(after, OpeningBook.Canonical(mirrored.Play(line.Player, reply.Square), line.Player.Opponent()).Key);
            }
        }

        Assert.True(positions > 11_000);
    }

    [Fact]
    public void TryGetMove_ReturnsFalseOutsideTheBook()
    {
        (Board board, Player player) = TestPositions.RandomPositions(seed: 5, count: 1, plies: 40).Single();

        Assert.False(BookTestData.Master.TryGetMove(board, player, new Random(0), out _));
    }

    [Fact]
    public void TryGetMove_PlaysTheMainLineFromTheStart()
    {
        var game = new Game();
        var random = new Random(0);
        while (BookTestData.Master.TryGetMove(game.Board, game.ToMove, random, out BookMove move))
        {
            game.Play(move.Square);
        }

        Assert.Equal("f5 d6 c3 d3 c4 f4 f6 g5 e6 f7 g6 c5 f3 e7 h6 g4 g3", GameRecordFormat.Format(game));
    }

    [Fact]
    public void Canonical_IsTheSameAfterEachOfTheFourFirstMoves()
    {
        BookKey[] keys = new[] { "d3", "c4", "f5", "e6" }
            .Select(first => OpeningBook.Canonical(TestGames.Play(first).Board, Player.White).Key)
            .ToArray();

        Assert.Single(keys.Distinct());
    }

    [Fact]
    public void ShippedBinaryBook_HoldsTheTextBook()
    {
        byte[] built = BookTestData.WriteBinary(OpeningBook.Load(BookTestData.TextPath));

        Assert.Equal(File.ReadAllBytes(BookTestData.BinaryPath), built);
    }

    [Fact]
    public void TextBook_IsInNormalForm()
    {
        string text = File.ReadAllText(BookTestData.TextPath).Replace("\r\n", "\n", StringComparison.Ordinal);

        Assert.Equal(text, BookTestData.WriteText(BookTestData.ReadText(text)));
    }

    [Fact]
    public void Load_ReadsTheTextAndTheBinaryBookToTheSameBook()
    {
        string fromText = BookTestData.WriteText(OpeningBook.Load(BookTestData.TextPath));

        Assert.Equal(fromText, BookTestData.WriteText(OpeningBook.Load(BookTestData.BinaryPath)));
    }

    [Fact]
    public void Save_KeepsValuesOriginsAndEfforts()
    {
        string text = Header +
            "d3 c5:-39:B c3:-40:H:60s:d14:v1 e3:-110:W:1500ms:d20:v1\n" +
            "d3c5 f6:39:X:solve:d58:v1 e6:12:H:8ply:d8:v1 d6:-16:H:-:d9:v1 c6:-56:U\n";
        OpeningBook book = BookTestData.ReadText(text);

        OpeningBook loaded = OpeningBook.Load(new MemoryStream(BookTestData.WriteBinary(book)));

        Assert.EndsWith(text[Header.Length..], BookTestData.WriteText(loaded));
        Assert.Equal(new BookEffort(EffortKind.Time, 1500, 20, 1), BookTestData.Replies(loaded, "d3")[2].Entry.Effort);
    }

    [Fact]
    public void Import_PlaysTheSameMovesAsTheOldBook()
    {
        var legacy = new LegacyLookup(BookTestData.LegacyPath);
        OpeningBook book = OpeningBook.Load(BookTestData.LegacyPath);
        int positions = 0;

        foreach ((Board board, Player player) in legacy.Positions.Where(p => p.Board.HasLegalMove(p.Player)))
        {
            positions++;
            Square? expected = legacy.Move(board, player);
            Assert.True(book.TryGetMove(board, player, new Random(0), out BookMove move));

            // The old book could store a position in more than one mirrored frame; then the first one is used now.
            if (move.Square != expected)
            {
                Assert.True(legacy.Frames(board, player) > 1);
            }
        }

        Assert.True(positions > 11_000);
    }

    [Fact]
    public void Import_StoresEachPositionOnce()
    {
        OpeningBook book = OpeningBook.Load(BookTestData.LegacyPath);

        Assert.Equal(11_200, book.PositionCount);
        Assert.Equal(22_878, book.NodeCount);
    }

    [Fact]
    public void Import_TakesTheOriginFromTheFlags()
    {
        // Flags: 1 = calculated, 2 = exact, 4 = win/loss/draw. C++ wrote 0 for the first value of a chain if it was 32600.
        byte[] data = BookTestData.Legacy(
            (Square.Parse("c5").ToLegacy(), 0, 1),
            (Square.Parse("c3").ToLegacy(), -40, 1),
            (Square.Parse("e3").ToLegacy(), -32610, 1 | 2),
            (Square.Parse("a1").ToLegacy(), 5, 1));

        OpeningBook book = OpeningBook.Load(new MemoryStream(data));

        Assert.Equal(
            [("c5", BookOrigin.Unknown), ("c3", BookOrigin.Heuristic), ("e3", BookOrigin.Exact)],
            BookTestData.Replies(book, "d3").Select(r => (r.Move.ToString(), r.Entry.Origin)));
    }

    [Fact]
    public void Import_MarksMovesFromGamesAsUnknown()
    {
        byte[] data = BookTestData.Legacy((Square.Parse("c5").ToLegacy(), 32665, 0), (Square.Parse("e3").ToLegacy(), -50, 1 | 4));

        OpeningBook book = OpeningBook.Load(new MemoryStream(data));

        Assert.Equal([BookOrigin.Unknown, BookOrigin.WinLossDraw], BookTestData.Replies(book, "d3").Select(r => r.Entry.Origin));
    }

    [Fact]
    public void LoadLegacy_RejectsTruncatedFile()
    {
        byte[] data = File.ReadAllBytes(BookTestData.LegacyPath)[..1000];

        Assert.Throws<InvalidDataException>(() => OpeningBook.Load(new MemoryStream(data)));
    }

    [Fact]
    public void LoadLegacy_RejectsNegativeChainLength()
    {
        byte[] data = [1, 0, 0, 0, 0xFF, 0xFF];

        Assert.Throws<InvalidDataException>(() => OpeningBook.Load(new MemoryStream(data)));
    }

    [Fact]
    public void LoadLegacy_RejectsTooDeepTree()
    {
        using var stream = new MemoryStream();
        using (var writer = new BinaryWriter(stream, System.Text.Encoding.UTF8, leaveOpen: true))
        {
            writer.Write(100);
            for (int i = 0; i < 100; i++)
            {
                writer.Write((short)1);
                writer.Write((short)34);
                writer.Write((short)0);
                writer.Write((short)0);
            }
        }

        stream.Position = 0;
        Assert.Throws<InvalidDataException>(() => OpeningBook.Load(stream));
    }

    [Fact]
    public void LoadBinary_RejectsTruncatedFile()
    {
        byte[] data = File.ReadAllBytes(BookTestData.BinaryPath)[..^1];

        Assert.Throws<InvalidDataException>(() => OpeningBook.Load(new MemoryStream(data)));
    }

    [Fact]
    public void LoadBinary_RejectsUnknownVersion()
    {
        byte[] data = File.ReadAllBytes(BookTestData.BinaryPath);
        data[4] = 3;

        Assert.Throws<InvalidDataException>(() => OpeningBook.Load(new MemoryStream(data)));
    }

    [Fact]
    public void LoadBinary_RejectsIllegalMove()
    {
        byte[] data = BookTestData.WriteBinary(BookTestData.ReadText(Header + "d3 c5:-39:U\n"));

        // Magic, version, two counts, the number of moves, then the first square.
        data[4 + 1 + 4 + 4 + 1] = (byte)Square.Parse("a1").Index;

        Assert.Throws<InvalidDataException>(() => OpeningBook.Load(new MemoryStream(data)));
    }

    [Theory]
    [InlineData("d3 a1:0:H", "a1 is not legal")]
    [InlineData("d4 c5:0:U", "starting with d3")]
    [InlineData("d3", "no book moves")]
    [InlineData("d3 c5:0:U\nd3 c3:0:U", "already in the book")]
    [InlineData("d3 c5:0:U c5:1:U", "given twice")]
    [InlineData("d3 c5:x:U", "not a book move")]
    [InlineData("d3 c5:0:Q", "not an origin")]
    [InlineData("d3 c5:0:U:60s:d3:v1", "search effort")]
    [InlineData("d3 c5:0:H:60:d3:v1", "search effort")]
    [InlineData("d3 c5:0:U\nd3c3 c4:0:U", "cannot be reached")]
    public void LoadText_RejectsInvalidBook(string lines, string message)
    {
        var exception = Assert.Throws<InvalidDataException>(() => BookTestData.ReadText(Header + lines + "\n"));

        Assert.Contains(message, exception.Message);
    }

    [Fact]
    public void LoadText_RequiresTheFirstLine()
    {
        Assert.Throws<InvalidDataException>(() => BookTestData.ReadText("d3 c5:0:U\n"));
    }
}

public class BookTrackerTests
{
    [Fact]
    public void ShouldConsult_UntilThreeMissesInARow()
    {
        var tracker = new BookTracker();

        tracker.Record(found: false);
        tracker.Record(found: false);
        Assert.True(tracker.ShouldConsult);

        tracker.Record(found: false);
        Assert.False(tracker.ShouldConsult);
    }

    [Fact]
    public void Record_HitStartsTheCountAgain()
    {
        var tracker = new BookTracker();
        tracker.Record(found: false);
        tracker.Record(found: false);

        tracker.Record(found: true);
        tracker.Record(found: false);
        tracker.Record(found: false);

        Assert.True(tracker.ShouldConsult);
    }

    [Fact]
    public void Reset_ConsultsTheBookAgain()
    {
        var tracker = new BookTracker();
        for (int i = 0; i < 3; i++)
        {
            tracker.Record(found: false);
        }

        tracker.Reset();

        Assert.True(tracker.ShouldConsult);
    }
}
