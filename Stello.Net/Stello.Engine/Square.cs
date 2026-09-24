using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace Stello.Engine;

/// <summary>A board square. <see cref="Index"/> = row * 8 + column, so a1 = 0 and h8 = 63.</summary>
public readonly record struct Square
{
    public Square(int index)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(index);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(index, 63);
        Index = index;
    }

    public int Index { get; }

    public int Column => Index & 7;

    public int Row => Index >> 3;

    public ulong Bit => 1UL << Index;

    public static Square At(int column, int row)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(column);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(column, 7);
        ArgumentOutOfRangeException.ThrowIfNegative(row);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(row, 7);
        return new Square(row * 8 + column);
    }

    // C++ square index: 10 * row + column, both 1-based (a1 = 11, h8 = 88).
    public int ToLegacy() => (Row + 1) * 10 + Column + 1;

    public static Square FromLegacy(int legacy)
    {
        int row = legacy / 10 - 1;
        int column = legacy % 10 - 1;
        if (legacy < 0 || row is < 0 or > 7 || column is < 0 or > 7)
        {
            throw new ArgumentOutOfRangeException(nameof(legacy), legacy, "Not a legacy board square (11-88).");
        }

        return At(column, row);
    }

    public static bool TryParse(string? text, [NotNullWhen(true)] out Square? square)
    {
        square = null;
        if (text is not { Length: 2 })
        {
            return false;
        }

        int column = char.ToLowerInvariant(text[0]) - 'a';
        int row = text[1] - '1';
        if (column is < 0 or > 7 || row is < 0 or > 7)
        {
            return false;
        }

        square = At(column, row);
        return true;
    }

    public static Square Parse(string text) =>
        TryParse(text, out Square? square)
            ? square.Value
            : throw new FormatException($"'{text}' is not a square (a1-h8).");

    public static IEnumerable<Square> InMask(ulong mask)
    {
        while (mask != 0)
        {
            yield return new Square(BitOperations.TrailingZeroCount(mask));
            mask &= mask - 1;
        }
    }

    public override string ToString() => $"{(char)('a' + Column)}{Row + 1}";
}
