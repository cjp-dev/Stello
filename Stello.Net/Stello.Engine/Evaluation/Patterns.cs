using System.Numerics;
using System.Runtime.CompilerServices;

namespace Stello.Engine.Evaluation;

/// <summary>
/// A pattern of squares in its canonical place on the board, such as the second row. The squares of a position are
/// read in the pattern's order as base-3 digits: empty 0, own disc 1, opponent's disc 2; the first square is the
/// lowest digit. The same pattern in its other places (the second row, the seventh row, the b and g columns) is read
/// from the position turned by a symmetry, so every place shares one weight table.
/// </summary>
internal sealed class PatternShape
{
    private const ulong DiagonalMultiplier = 0x0101_0101_0101_0101UL;

    private readonly Part[] _parts;

    public PatternShape(string name, params int[] squares)
    {
        ArgumentOutOfRangeException.ThrowIfGreaterThan(squares.Length, 10);
        if (squares.Distinct().Count() != squares.Length || squares.Any(s => s is < 0 or > 63))
        {
            throw new ArgumentException($"Pattern {name} has an invalid list of squares.", nameof(squares));
        }

        Name = name;
        Squares = squares;
        Size = (int)Math.Pow(3, squares.Length);
        _parts = MakeParts(squares);
        Placements = FindPlacements(squares);
        (Fold, Representatives) = MakeFold(squares, Size);
    }

    public string Name { get; }

    /// <summary>The squares in digit order (the first is the lowest digit).</summary>
    public IReadOnlyList<int> Squares { get; }

    /// <summary>3 to the power of the number of squares: the number of raw indices.</summary>
    public int Size { get; }

    /// <summary>
    /// The symmetries (<see cref="Symmetry"/>) whose turned positions are read: one for each different set of squares
    /// the pattern covers on the board.
    /// </summary>
    public IReadOnlyList<int> Placements { get; }

    /// <summary>
    /// For every raw index, the smallest raw index that must have the same weight because a symmetry maps the
    /// pattern's squares onto themselves in another order (for example a row read backwards).
    /// </summary>
    public IReadOnlyList<int> Fold { get; }

    /// <summary>The raw indices that are their own fold, in increasing order: the weights a file stores.</summary>
    public IReadOnlyList<int> Representatives { get; }

    /// <summary>The raw index of the canonical squares of a position (own and opponent's discs, already turned).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public int Index(ulong own, ulong opponent)
    {
        int index = 0;
        foreach (Part part in _parts)
        {
            index += part.Digits[part.Extract(own)] + 2 * part.Digits[part.Extract(opponent)];
        }

        return index;
    }

    /// <summary>The raw index read square by square: slow, for tests and checks.</summary>
    public int SlowIndex(ulong own, ulong opponent)
    {
        int index = 0;
        int power = 1;
        foreach (int square in Squares)
        {
            ulong bit = 1UL << square;
            index += power * ((own & bit) != 0 ? 1 : (opponent & bit) != 0 ? 2 : 0);
            power *= 3;
        }

        return index;
    }

    public override string ToString() => Name;

    /// <summary>
    /// Squares in one row are read with a shift; squares on one diagonal (column + row the same, or column − row the
    /// same) are gathered into the top byte with a multiplication. Each part's byte is turned into base-3 digits with a
    /// table.
    /// </summary>
    private static Part[] MakeParts(int[] squares)
    {
        List<int[]> groups;
        bool sameRow = squares.All(s => s >> 3 == squares[0] >> 3);
        bool antiDiagonal = squares.All(s => (s & 7) + (s >> 3) == (squares[0] & 7) + (squares[0] >> 3));
        bool diagonal = squares.All(s => (s & 7) - (s >> 3) == (squares[0] & 7) - (squares[0] >> 3));
        groups = !sameRow && (antiDiagonal || diagonal)
            ? [squares]
            : squares.GroupBy(s => s >> 3).Select(g => g.ToArray()).ToList();

        var parts = new List<Part>();
        foreach (int[] group in groups)
        {
            bool row = group.All(s => s >> 3 == group[0] >> 3);
            ulong mask = group.Aggregate(0UL, (m, s) => m | (1UL << s));
            var part = new Part(mask, row ? 1 : DiagonalMultiplier, row ? 8 * (group[0] >> 3) : 56, new int[256]);

            // Where each square's bit lands in the part's byte, and its digit value 3^(position in the pattern).
            var bitOf = new int[group.Length];
            for (int i = 0; i < group.Length; i++)
            {
                int value = part.Extract(1UL << group[i]);
                if (BitOperations.PopCount((uint)value) != 1)
                {
                    throw new InvalidOperationException($"Square {group[i]} cannot be extracted.");
                }

                bitOf[i] = BitOperations.TrailingZeroCount(value);
            }

            if (bitOf.Distinct().Count() != bitOf.Length)
            {
                throw new InvalidOperationException("Two squares of a part land on the same bit.");
            }

            for (int subset = 0; subset < 1 << group.Length; subset++)
            {
                ulong discs = 0;
                int expected = 0;
                int digits = 0;
                for (int i = 0; i < group.Length; i++)
                {
                    if ((subset & (1 << i)) != 0)
                    {
                        discs |= 1UL << group[i];
                        expected |= 1 << bitOf[i];
                        digits += (int)Math.Pow(3, Array.IndexOf(squares, group[i]));
                    }
                }

                int extracted = part.Extract(discs);
                if (extracted != expected)
                {
                    throw new InvalidOperationException("A part's extraction is not carry-free.");
                }

                part.Digits[extracted] = digits;
            }

            parts.Add(part);
        }

        return parts.ToArray();
    }

    private static int[] FindPlacements(int[] squares)
    {
        var seen = new HashSet<ulong>();
        var placements = new List<int>();
        for (int symmetry = 0; symmetry < Symmetry.Count; symmetry++)
        {
            // The squares of the original position that the turned position has on the pattern's squares.
            ulong covered = 0;
            for (int square = 0; square < 64; square++)
            {
                if (squares.Contains(Symmetry.MapSquare(square, symmetry)))
                {
                    covered |= 1UL << square;
                }
            }

            if (seen.Add(covered))
            {
                placements.Add(symmetry);
            }
        }

        return placements.ToArray();
    }

    private static (int[] Fold, int[] Representatives) MakeFold(int[] squares, int size)
    {
        // The permutations of the pattern's squares by the symmetries that map the set of squares onto itself.
        var permutations = new List<int[]>();
        for (int symmetry = 1; symmetry < Symmetry.Count; symmetry++)
        {
            int[] mapped = squares.Select(s => Array.IndexOf(squares, Symmetry.MapSquare(s, symmetry))).ToArray();
            if (mapped.All(i => i >= 0))
            {
                permutations.Add(mapped);
            }
        }

        var fold = new int[size];
        var digits = new int[squares.Length];
        for (int index = 0; index < size; index++)
        {
            for (int i = 0, rest = index; i < squares.Length; i++, rest /= 3)
            {
                digits[i] = rest % 3;
            }

            int smallest = index;
            foreach (int[] permutation in permutations)
            {
                int other = 0;
                for (int i = 0, power = 1; i < squares.Length; i++, power *= 3)
                {
                    // Square i's digit moves to the place of the square it is mapped onto.
                    other += digits[Array.IndexOf(permutation, i)] * power;
                }

                smallest = Math.Min(smallest, other);
            }

            fold[index] = smallest;
        }

        int[] representatives = Enumerable.Range(0, size).Where(i => fold[i] == i).ToArray();
        return (fold, representatives);
    }

    private readonly record struct Part(ulong Mask, ulong Multiplier, int Shift, int[] Digits)
    {
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public int Extract(ulong discs) => (int)(((discs & Mask) * Multiplier >> Shift) & 0xFF);
    }
}

/// <summary>
/// The patterns of an evaluation, each in all its places on the board: a feature is one pattern in one place.
/// </summary>
internal sealed class PatternSet
{
    private readonly (PatternShape Shape, int Symmetry, int Offset)[] _features;
    private readonly int[] _symmetries;

    public PatternSet(string name, params PatternShape[] shapes)
    {
        Name = name;
        Shapes = shapes;
        var offsets = new int[shapes.Length];
        for (int i = 1; i < shapes.Length; i++)
        {
            offsets[i] = offsets[i - 1] + shapes[i - 1].Size;
        }

        Offsets = offsets;
        RawSize = offsets[^1] + shapes[^1].Size;
        _features = shapes.SelectMany((shape, i) => shape.Placements.Select(symmetry => (shape, symmetry, offsets[i]))).ToArray();
        _symmetries = _features.Select(f => f.Symmetry).Distinct().Order().ToArray();
    }

    public string Name { get; }

    public IReadOnlyList<PatternShape> Shapes { get; }

    /// <summary>Where each shape's raw indices start in one table of all shapes.</summary>
    public IReadOnlyList<int> Offsets { get; }

    /// <summary>The number of raw indices of all shapes together.</summary>
    public int RawSize { get; }

    public int FeatureCount => _features.Length;

    /// <summary>The shape of each feature, by feature number.</summary>
    public PatternShape ShapeOf(int feature) => _features[feature].Shape;

    /// <summary>
    /// The 11 patterns of Logistello and Edax-like programs: rows 2–4, the diagonals of 4 to 8 squares, the edge
    /// with both X-squares, the 2 × 5 corner and the 3 × 3 corner; 46 features.
    /// </summary>
    public static PatternSet Classic11 { get; } = new("classic11",
        new PatternShape("row2", 8, 9, 10, 11, 12, 13, 14, 15),
        new PatternShape("row3", 16, 17, 18, 19, 20, 21, 22, 23),
        new PatternShape("row4", 24, 25, 26, 27, 28, 29, 30, 31),
        new PatternShape("diagonal4", 3, 10, 17, 24),
        new PatternShape("diagonal5", 4, 11, 18, 25, 32),
        new PatternShape("diagonal6", 5, 12, 19, 26, 33, 40),
        new PatternShape("diagonal7", 6, 13, 20, 27, 34, 41, 48),
        new PatternShape("diagonal8", 7, 14, 21, 28, 35, 42, 49, 56),
        new PatternShape("edge+2x", 0, 1, 2, 3, 4, 5, 6, 7, 9, 14),
        new PatternShape("corner2x5", 0, 1, 2, 3, 4, 8, 9, 10, 11, 12),
        new PatternShape("corner3x3", 0, 1, 2, 8, 9, 10, 16, 17, 18));

    /// <summary>The pattern sets by name, for files and the trainer.</summary>
    public static PatternSet ByName(string name) => name switch
    {
        "classic11" => Classic11,
        _ => throw new ArgumentException($"Unknown pattern set {name}."),
    };

    /// <summary>
    /// Writes the raw index of every feature into <paramref name="indices"/>, each plus its shape's offset in
    /// <see cref="Offsets"/>, so that it indexes one table of all shapes.
    /// </summary>
    public void Indices(ulong own, ulong opponent, Span<int> indices)
    {
        Span<ulong> turnedOwn = stackalloc ulong[Symmetry.Count];
        Span<ulong> turnedOpponent = stackalloc ulong[Symmetry.Count];
        foreach (int symmetry in _symmetries)
        {
            turnedOwn[symmetry] = Symmetry.Apply(own, symmetry);
            turnedOpponent[symmetry] = Symmetry.Apply(opponent, symmetry);
        }

        for (int i = 0; i < _features.Length; i++)
        {
            (PatternShape shape, int symmetry, int offset) = _features[i];
            indices[i] = offset + shape.Index(turnedOwn[symmetry], turnedOpponent[symmetry]);
        }
    }
}
