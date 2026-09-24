namespace Stello.Engine.Tests;

public class OpeningBookTests
{
    private static readonly string BookPath = Path.Combine(AppContext.BaseDirectory, "Data", "OPENING");
    private static readonly Lazy<OpeningBook> MasterBook = new(() => OpeningBook.Load(BookPath));

    [Fact]
    public void Load_ReadsAllNodes()
    {
        // File size = 4 (header) + 2 (root chain) + 8 per node (6 for the node, 2 for its reply chain).
        long nodes = (new FileInfo(BookPath).Length - 6) / 8;

        Assert.Equal(nodes, MasterBook.Value.NodeCount);
    }

    [Fact]
    public void Save_AfterLoadGivesIdenticalFile()
    {
        using var stream = new MemoryStream();

        MasterBook.Value.Save(stream);

        Assert.Equal(File.ReadAllBytes(BookPath), stream.ToArray());
    }

    [Fact]
    public void TryGetMove_ChoosesBlacksFirstMoveAtRandom()
    {
        var moves = new HashSet<string>();
        for (int seed = 0; seed < 100; seed++)
        {
            Assert.True(MasterBook.Value.TryGetMove(Board.Initial, Player.Black, new Random(seed), out BookMove move));
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

        Assert.True(MasterBook.Value.TryGetMove(board, Player.White, new Random(0), out BookMove move));

        Assert.Equal(Square.Parse(reply), move.Square);
        Assert.Equal(-39, move.Value);
    }

    [Fact]
    public void TryGetMove_GivesLegalMoveInEveryBookPosition()
    {
        OpeningBook book = MasterBook.Value;
        int positions = 0;

        void Visit(List<BookNode> replies, Board board, Player player)
        {
            if (replies.Count == 0 || !board.HasLegalMove(player))
            {
                return;
            }

            positions++;
            Assert.True(book.TryGetMove(board, player, new Random(0), out BookMove move));
            Assert.True(board.IsLegal(player, move.Square));

            foreach (BookNode node in replies.Where(n => n.Move != 0))
            {
                Square square = Square.FromLegacy(node.Move);
                if (board.IsLegal(player, square))
                {
                    Visit(node.Children, board.Play(player, square), player.Opponent());
                }
            }
        }

        Visit(book.Root, TestGames.Play("d3").Board, Player.White);

        Assert.True(positions > 1000);
    }

    [Fact]
    public void TryGetMove_ReturnsFalseOutsideTheBook()
    {
        (Board board, Player player) = TestPositions.RandomPositions(seed: 5, count: 1, plies: 40).Single();

        Assert.False(MasterBook.Value.TryGetMove(board, player, new Random(0), out _));
    }

    [Fact]
    public void Save_WritesZeroForFirstNodeWithValue32600()
    {
        // Kept from C++ savebook.
        byte[] data = Bytes(header: 2, chain: [(35, 32600), (53, 32600)]);
        using var output = new MemoryStream();

        OpeningBook.Load(new MemoryStream(data)).Save(output);

        byte[] expected = Bytes(header: 2, chain: [(35, 0), (53, 32600)]);
        Assert.Equal(expected, output.ToArray());
    }

    [Fact]
    public void Load_RejectsTruncatedFile()
    {
        byte[] data = File.ReadAllBytes(BookPath)[..1000];

        Assert.Throws<InvalidDataException>(() => OpeningBook.Load(new MemoryStream(data)));
    }

    [Fact]
    public void Load_RejectsNegativeChainLength()
    {
        byte[] data = [1, 0, 0, 0, 0xFF, 0xFF];

        Assert.Throws<InvalidDataException>(() => OpeningBook.Load(new MemoryStream(data)));
    }

    [Fact]
    public void Load_RejectsTooDeepTree()
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

    private static byte[] Bytes(int header, (short Move, short Value)[] chain)
    {
        using var stream = new MemoryStream();
        using (var writer = new BinaryWriter(stream))
        {
            writer.Write(header);
            writer.Write((short)chain.Length);
            foreach ((short move, short value) in chain)
            {
                writer.Write(move);
                writer.Write(value);
                writer.Write((short)0);
                writer.Write((short)0);
            }
        }

        return stream.ToArray();
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
