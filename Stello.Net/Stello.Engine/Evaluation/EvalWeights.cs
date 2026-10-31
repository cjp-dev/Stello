using System.IO.Compression;
using System.Runtime.CompilerServices;
using System.Text;

namespace Stello.Engine.Evaluation;

/// <summary>
/// Trained pattern weights: for every stage of the game (a range of disc counts), one weight per raw index of every
/// pattern, in 1/<see cref="UnitsPerDisc"/> discs, for the side to move.
/// </summary>
/// <remarks>
/// The file (made by tools/Stello.Trainer) is gzip-compressed. Inside, all numbers are little-endian:
/// <c>"STELLOEV"</c>, version (u16, 2), pattern set name (u8 length and ASCII), number of stages (u8), the stage of
/// every disc count 0–64 (65 × u8), units per disc (u16), then for every stage its folded weights: one i16 per
/// <see cref="PatternShape.Representatives"/> entry of every pattern, minus the same weight of the stage before (so
/// neighbouring stages, which are alike, give small numbers), written as all low bytes and then all high bytes (the
/// high bytes are mostly 0 or 0xFF and compress well).
/// </remarks>
internal sealed class EvalWeights
{
    private const string Magic = "STELLOEV";
    private const int Version = 2;

    private readonly short[][] _weights;
    private readonly byte[] _stageOfDiscs;

    public EvalWeights(PatternSet set, byte[] stageOfDiscs, int unitsPerDisc, short[][] weights)
    {
        if (stageOfDiscs.Length != 65 || weights.Length == 0 || weights.Any(w => w.Length != set.RawSize)
            || stageOfDiscs.Any(s => s >= weights.Length) || unitsPerDisc is < 1 or > 1024)
        {
            throw new ArgumentException("The weights do not fit the pattern set.");
        }

        Set = set;
        _stageOfDiscs = stageOfDiscs;
        UnitsPerDisc = unitsPerDisc;
        _weights = weights;
    }

    public PatternSet Set { get; }

    public int Stages => _weights.Length;

    public int UnitsPerDisc { get; }

    public int StageOf(int discs) => _stageOfDiscs[discs];

    /// <summary>The weights of one stage, by raw index plus the shape's offset (<see cref="PatternSet.Offsets"/>).</summary>
    public ReadOnlySpan<short> StageWeights(int stage) => _weights[stage];

    /// <summary>The evaluation for the side to move, in 1/<see cref="UnitsPerDisc"/> discs.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public int Evaluate(ulong own, ulong opponent)
    {
        Span<int> indices = stackalloc int[Set.FeatureCount];
        Set.Indices(own, opponent, indices);
        short[] weights = _weights[_stageOfDiscs[System.Numerics.BitOperations.PopCount(own | opponent)]];
        int sum = 0;
        foreach (int index in indices)
        {
            sum += weights[index];
        }

        return sum;
    }

    /// <summary>The stage of each disc count when the game (4 to 64 discs) is cut into <paramref name="stages"/> equal parts.</summary>
    public static byte[] EvenStages(int stages)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(stages, 1);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(stages, 61);
        var stageOf = new byte[65];
        for (int discs = 0; discs <= 64; discs++)
        {
            stageOf[discs] = (byte)Math.Clamp((discs - 4) * stages / 61, 0, stages - 1);
        }

        return stageOf;
    }

    public static EvalWeights Load(string path)
    {
        using FileStream file = File.OpenRead(path);
        return Read(file);
    }

    /// <exception cref="InvalidDataException">The stream is not an evaluation file.</exception>
    public static EvalWeights Read(Stream stream)
    {
        using var gzip = new GZipStream(stream, CompressionMode.Decompress, leaveOpen: true);
        using var reader = new BinaryReader(gzip, Encoding.ASCII, leaveOpen: true);
        try
        {
            if (Encoding.ASCII.GetString(reader.ReadBytes(Magic.Length)) != Magic || reader.ReadUInt16() != Version)
            {
                throw new InvalidDataException("Not a Stello evaluation file (version 2).");
            }

            PatternSet set;
            try
            {
                set = PatternSet.ByName(Encoding.ASCII.GetString(reader.ReadBytes(reader.ReadByte())));
            }
            catch (ArgumentException exception)
            {
                throw new InvalidDataException(exception.Message, exception);
            }

            int stages = reader.ReadByte();
            byte[] stageOfDiscs = reader.ReadBytes(65);
            int unitsPerDisc = reader.ReadUInt16();
            int count = set.Shapes.Sum(shape => shape.Representatives.Count);
            var weights = new short[stages][];
            var previous = new short[count];
            for (int stage = 0; stage < stages; stage++)
            {
                byte[] low = ReadExactly(reader, count);
                byte[] high = ReadExactly(reader, count);
                for (int i = 0; i < count; i++)
                {
                    previous[i] = (short)(previous[i] + (short)(low[i] | (high[i] << 8)));
                }

                weights[stage] = new short[set.RawSize];
                int next = 0;
                for (int s = 0; s < set.Shapes.Count; s++)
                {
                    PatternShape shape = set.Shapes[s];
                    var folded = new short[shape.Size];
                    foreach (int representative in shape.Representatives)
                    {
                        folded[representative] = previous[next++];
                    }

                    Span<short> table = weights[stage].AsSpan(set.Offsets[s], shape.Size);
                    for (int raw = 0; raw < shape.Size; raw++)
                    {
                        table[raw] = folded[shape.Fold[raw]];
                    }
                }
            }

            return new EvalWeights(set, stageOfDiscs, unitsPerDisc, weights);
        }
        catch (EndOfStreamException exception)
        {
            throw new InvalidDataException("The evaluation file is cut short.", exception);
        }
        catch (ArgumentException exception)
        {
            throw new InvalidDataException(exception.Message, exception);
        }
    }

    public void Write(Stream stream)
    {
        using var gzip = new GZipStream(stream, CompressionLevel.SmallestSize, leaveOpen: true);
        using var writer = new BinaryWriter(gzip, Encoding.ASCII, leaveOpen: true);
        writer.Write(Encoding.ASCII.GetBytes(Magic));
        writer.Write((ushort)Version);
        writer.Write((byte)Set.Name.Length);
        writer.Write(Encoding.ASCII.GetBytes(Set.Name));
        writer.Write((byte)Stages);
        writer.Write(_stageOfDiscs);
        writer.Write((ushort)UnitsPerDisc);
        short[]? previous = null;
        for (int stage = 0; stage < Stages; stage++)
        {
            short[] folded = Set.Shapes
                .SelectMany((shape, s) => shape.Representatives.Select(representative => _weights[stage][Set.Offsets[s] + representative]))
                .ToArray();
            var difference = new short[folded.Length];
            for (int i = 0; i < folded.Length; i++)
            {
                difference[i] = (short)(folded[i] - (previous?[i] ?? 0));
            }

            writer.Write(difference.Select(d => (byte)d).ToArray());
            writer.Write(difference.Select(d => (byte)(d >> 8)).ToArray());
            previous = folded;
        }
    }

    private static byte[] ReadExactly(BinaryReader reader, int count)
    {
        byte[] bytes = reader.ReadBytes(count);
        return bytes.Length == count ? bytes : throw new EndOfStreamException();
    }
}
