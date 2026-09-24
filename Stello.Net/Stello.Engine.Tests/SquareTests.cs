namespace Stello.Engine.Tests;

public class SquareTests
{
    [Theory]
    [InlineData("a1", 0, 11)]
    [InlineData("h1", 7, 18)]
    [InlineData("a8", 56, 81)]
    [InlineData("h8", 63, 88)]
    [InlineData("d3", 19, 34)]
    [InlineData("c4", 26, 43)]
    [InlineData("f5", 37, 56)]
    [InlineData("e6", 44, 65)]
    public void Parse_GivesIndexAndLegacyIndex(string text, int index, int legacy)
    {
        Square square = Square.Parse(text);

        Assert.Equal(index, square.Index);
        Assert.Equal(legacy, square.ToLegacy());
        Assert.Equal(square, Square.FromLegacy(legacy));
        Assert.Equal(text, square.ToString());
    }

    [Fact]
    public void Legacy_RoundTripsForAllSquares()
    {
        for (int index = 0; index < 64; index++)
        {
            var square = new Square(index);
            Assert.Equal(square, Square.FromLegacy(square.ToLegacy()));
        }
    }

    [Theory]
    [InlineData(-1)]
    [InlineData(0)]
    [InlineData(10)]
    [InlineData(19)]
    [InlineData(20)]
    [InlineData(89)]
    [InlineData(90)]
    [InlineData(99)]
    public void FromLegacy_RejectsBorderSquares(int legacy)
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => Square.FromLegacy(legacy));
    }

    [Theory]
    [InlineData(null)]
    [InlineData("")]
    [InlineData("a")]
    [InlineData("a0")]
    [InlineData("a9")]
    [InlineData("i1")]
    [InlineData("a10")]
    public void TryParse_RejectsInvalidText(string? text)
    {
        Assert.False(Square.TryParse(text, out _));
    }

    [Fact]
    public void TryParse_AcceptsUpperCase()
    {
        Assert.True(Square.TryParse("F5", out Square? square));
        Assert.Equal(Square.Parse("f5"), square);
    }

    [Fact]
    public void InMask_EnumeratesSetBits()
    {
        ulong mask = Square.Parse("a1").Bit | Square.Parse("e4").Bit | Square.Parse("h8").Bit;

        Assert.Equal(["a1", "e4", "h8"], Square.InMask(mask).Select(s => s.ToString()));
    }
}
