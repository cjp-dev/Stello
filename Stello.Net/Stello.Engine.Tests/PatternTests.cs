using Stello.Engine.Evaluation;

namespace Stello.Engine.Tests;

public class PatternTests
{
    [Fact]
    public void Symmetry_MapsSquaresAsDocumented()
    {
        for (int square = 0; square < 64; square++)
        {
            int column = square & 7;
            int row = square >> 3;
            Assert.Equal(row * 8 + (7 - column), Symmetry.MapSquare(square, 1));
            Assert.Equal((7 - row) * 8 + column, Symmetry.MapSquare(square, 2));
            Assert.Equal(column * 8 + row, Symmetry.MapSquare(square, 4));
        }
    }

    [Fact]
    public void Symmetry_TurnsWholeBoardsSquareBySquare()
    {
        var random = new Random(3);
        for (int test = 0; test < 200; test++)
        {
            ulong x = (ulong)random.NextInt64() ^ ((ulong)random.NextInt64() << 1);
            for (int symmetry = 0; symmetry < Symmetry.Count; symmetry++)
            {
                ulong expected = 0;
                for (int square = 0; square < 64; square++)
                {
                    if ((x & (1UL << square)) != 0)
                    {
                        expected |= 1UL << Symmetry.MapSquare(square, symmetry);
                    }
                }

                Assert.Equal(expected, Symmetry.Apply(x, symmetry));
            }
        }
    }

    [Fact]
    public void Classic11_Has46FeaturesAndTheExpectedFoldedSizes()
    {
        PatternSet set = PatternSet.Classic11;

        Assert.Equal(46, set.FeatureCount);
        Assert.Equal([4, 4, 4, 4, 4, 4, 4, 2, 4, 8, 4], set.Shapes.Select(s => s.Placements.Count));
        Assert.Equal((6561 + 81) / 2, set.Shapes[0].Representatives.Count); // a row read backwards
        Assert.Equal(59049, set.Shapes[9].Representatives.Count); // the 2 × 5 corner has no symmetry of its own
        Assert.Equal((19683 + 729) / 2, set.Shapes[10].Representatives.Count); // the 3 × 3 corner mirrored in its diagonal
    }

    [Fact]
    public void LargerSets_HaveTheExpectedFeatures()
    {
        Assert.Equal(62, PatternSet.Plus15.FeatureCount);
        Assert.Equal(64, PatternSet.Egaroucid16.FeatureCount);
    }

    [Theory]
    [InlineData("classic11")]
    [InlineData("plus15")]
    [InlineData("egaroucid16")]
    public void Index_EqualsReadingSquareBySquare(string setName)
    {
        foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 11, count: 300, plies: 30))
        {
            ulong own = board.Discs(player);
            ulong opponent = board.Discs(player.Opponent());
            foreach (PatternShape shape in PatternSet.ByName(setName).Shapes)
            {
                for (int symmetry = 0; symmetry < Symmetry.Count; symmetry++)
                {
                    ulong turnedOwn = Symmetry.Apply(own, symmetry);
                    ulong turnedOpponent = Symmetry.Apply(opponent, symmetry);
                    Assert.Equal(shape.SlowIndex(turnedOwn, turnedOpponent), shape.Index(turnedOwn, turnedOpponent));
                }
            }
        }
    }

    [Theory]
    [InlineData("classic11")]
    [InlineData("plus15")]
    [InlineData("egaroucid16")]
    public void FoldedWeights_GiveTheSameSumForAllSymmetricPositions(string setName)
    {
        PatternSet set = PatternSet.ByName(setName);
        var random = new Random(5);
        var weights = new double[set.RawSize];
        for (int s = 0; s < set.Shapes.Count; s++)
        {
            PatternShape shape = set.Shapes[s];
            var folded = new double[shape.Size];
            foreach (int representative in shape.Representatives)
            {
                folded[representative] = random.NextDouble();
            }

            for (int raw = 0; raw < shape.Size; raw++)
            {
                weights[set.Offsets[s] + raw] = folded[shape.Fold[raw]];
            }
        }

        var indices = new int[set.FeatureCount];
        foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 12, count: 200, plies: 25))
        {
            double Sum(ulong own, ulong opponent)
            {
                set.Indices(own, opponent, indices);
                return indices.Sum(i => weights[i]);
            }

            ulong own = board.Discs(player);
            ulong opponent = board.Discs(player.Opponent());
            double expected = Sum(own, opponent);
            for (int symmetry = 1; symmetry < Symmetry.Count; symmetry++)
            {
                Assert.Equal(expected, Sum(Symmetry.Apply(own, symmetry), Symmetry.Apply(opponent, symmetry)), 9);
            }
        }
    }

    [Fact]
    public void EvalWeights_WriteAndRead_GiveTheSameEvaluation()
    {
        PatternSet set = PatternSet.Classic11;
        var random = new Random(9);
        byte[] stageOf = EvalWeights.EvenStages(3);
        var tables = new short[3][];
        for (int stage = 0; stage < 3; stage++)
        {
            tables[stage] = new short[set.RawSize];
            for (int s = 0; s < set.Shapes.Count; s++)
            {
                PatternShape shape = set.Shapes[s];
                var folded = new short[shape.Size];
                foreach (int representative in shape.Representatives)
                {
                    folded[representative] = (short)random.Next(-500, 500);
                }

                for (int raw = 0; raw < shape.Size; raw++)
                {
                    tables[stage][set.Offsets[s] + raw] = folded[shape.Fold[raw]];
                }
            }
        }

        var written = new EvalWeights(set, stageOf, 32, tables);
        using var stream = new MemoryStream();
        written.Write(stream);
        stream.Position = 0;
        EvalWeights read = EvalWeights.Read(stream);

        Assert.Equal(3, read.Stages);
        Assert.Equal(32, read.UnitsPerDisc);
        foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 13, count: 100, plies: 40))
        {
            ulong own = board.Discs(player);
            ulong opponent = board.Discs(player.Opponent());
            Assert.Equal(written.Evaluate(own, opponent), read.Evaluate(own, opponent));
        }
    }

    [Fact]
    public void EvalWeights_Read_RejectsOtherFiles()
    {
        using var stream = new MemoryStream();
        using (var gzip = new System.IO.Compression.GZipStream(stream, System.IO.Compression.CompressionLevel.Fastest, leaveOpen: true))
        {
            gzip.Write("NOTSTELLO"u8);
        }

        stream.Position = 0;
        Assert.Throws<InvalidDataException>(() => EvalWeights.Read(stream));
    }
}
