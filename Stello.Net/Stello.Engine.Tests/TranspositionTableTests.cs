using Stello.Engine.Search;

namespace Stello.Engine.Tests;

public class TranspositionTableTests
{
    [Fact]
    public void Store_ThenTryGetFindsTheEntry()
    {
        var table = new TranspositionTable(10);

        table.Store(0x1, 0x2, tag: 1, depth: 5, Bound.Exact, value: 42, move: 19);

        Assert.True(table.TryGet(0x1, 0x2, 1, out TranspositionTable.Entry entry));
        Assert.Equal((short)42, entry.Value);
        Assert.Equal((sbyte)5, entry.Depth);
        Assert.Equal(Bound.Exact, entry.Bound);
        Assert.Equal((sbyte)19, entry.Move);
    }

    [Fact]
    public void TryGet_OtherPositionOrTagIsNotFound()
    {
        var table = new TranspositionTable(10);
        table.Store(0x1, 0x2, tag: 1, depth: 5, Bound.Exact, value: 42, move: 19);

        Assert.False(table.TryGet(0x1, 0x2, 0, out _));
        Assert.False(table.TryGet(0x2, 0x1, 1, out _));
    }

    [Fact]
    public void Store_KeepsTheDeeperResultForTheSamePosition()
    {
        var table = new TranspositionTable(10);
        table.Store(0x1, 0x2, 0, depth: 9, Bound.Lower, value: 10, move: 1);

        table.Store(0x1, 0x2, 0, depth: 3, Bound.Upper, value: -10, move: 2);

        Assert.True(table.TryGet(0x1, 0x2, 0, out TranspositionTable.Entry entry));
        Assert.Equal((sbyte)9, entry.Depth);

        table.Store(0x1, 0x2, 0, depth: 12, Bound.Exact, value: 7, move: 3);

        Assert.True(table.TryGet(0x1, 0x2, 0, out entry));
        Assert.Equal((sbyte)12, entry.Depth);
    }

    [Fact]
    public void Store_DeepEntrySurvivesManyShallowOnes()
    {
        // Two slots only, so the shallow entries collide with the deep one.
        var table = new TranspositionTable(1);
        table.Store(0xFF, 0xFF00, 0, depth: 20, Bound.Exact, value: 1, move: 0);

        var random = new Random(3);
        ulong own = 0;
        ulong opponent = 0;
        for (int i = 0; i < 100; i++)
        {
            own = (ulong)random.NextInt64();
            opponent = (ulong)random.NextInt64() & ~own;
            table.Store(own, opponent, 0, depth: 1, Bound.Upper, value: 0, move: 0);
        }

        Assert.True(table.TryGet(0xFF, 0xFF00, 0, out _));
        Assert.True(table.TryGet(own, opponent, 0, out _));
    }

    [Fact]
    public void Clear_RemovesAllEntries()
    {
        var table = new TranspositionTable(10);
        table.Store(0x1, 0x2, 0, depth: 5, Bound.Exact, value: 42, move: 19);

        table.Clear();

        Assert.False(table.TryGet(0x1, 0x2, 0, out _));
    }
}
