namespace Stello.Engine.Tests;

public class MoveTests
{
    [Theory]
    [InlineData("pass")]
    [InlineData("PASS")]
    public void TryParse_ReadsPass(string text)
    {
        Assert.True(Move.TryParse(text, out Move? move));
        Assert.True(move.Value.IsPass);
        Assert.Equal("pass", move.Value.ToString());
    }

    [Fact]
    public void TryParse_ReadsSquare()
    {
        Assert.True(Move.TryParse("F5", out Move? move));
        Assert.Equal(new Move(Square.Parse("f5")), move);
        Assert.Equal("f5", move.Value.ToString());
    }

    [Theory]
    [InlineData(null)]
    [InlineData("")]
    [InlineData("passes")]
    [InlineData("z9")]
    public void TryParse_RejectsInvalidText(string? text)
    {
        Assert.False(Move.TryParse(text, out _));
    }
}
