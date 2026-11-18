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
/// <c>"STELLOEV"</c>, version (u16, 2 or 3), pattern set name (u8 length and ASCII), number of stages (u8), the stage
/// of every disc count 0–64 (65 × u8), units per disc (u16), then for every stage its folded weights: one i16 per
/// <see cref="PatternShape.Representatives"/> entry of every pattern, minus the same weight of the stage before (so
/// neighbouring stages, which are alike, give small numbers), written as all low bytes and then all high bytes (the
/// high bytes are mostly 0 or 0xFF and compress well). Version 3 adds 96 mobility weights per stage (48 for own legal
/// moves, 48 for opponent legal moves) in the same delta-encoded low/high byte layout after the pattern stages.
/// </remarks>
internal sealed class EvalWeights
{
    public const int MobilitySlotsPerSide = 48;
    public const int MobilityWeightsPerStage = 2 * MobilitySlotsPerSide;

    private const string Magic = "STELLOEV";
    private const int Version = 2;
    private const int MobilityVersion = 3;

    private readonly short[][] _weights;
    private readonly short[][]? _mobilityWeights;
    private readonly byte[] _stageOfDiscs;

    public EvalWeights(PatternSet set, byte[] stageOfDiscs, int unitsPerDisc, short[][] weights, short[][]? mobilityWeights = null)
    {
        if (stageOfDiscs.Length != 65 || weights.Length == 0 || weights.Any(w => w.Length != set.RawSize)
            || stageOfDiscs.Any(s => s >= weights.Length) || unitsPerDisc is < 1 or > 1024
            || (mobilityWeights is not null && (mobilityWeights.Length != weights.Length || mobilityWeights.Any(w => w.Length != MobilityWeightsPerStage))))
        {
            throw new ArgumentException("The weights do not fit the pattern set.");
        }

        Set = set;
        _stageOfDiscs = stageOfDiscs;
        UnitsPerDisc = unitsPerDisc;
        _weights = weights;
        _mobilityWeights = mobilityWeights;
    }

    public PatternSet Set { get; }

    public int Stages => _weights.Length;

    public int UnitsPerDisc { get; }

    public bool HasMobility => _mobilityWeights is not null;

    public int StageOf(int discs) => _stageOfDiscs[discs];

    /// <summary>The weights of one stage, by raw index plus the shape's offset (<see cref="PatternSet.Offsets"/>).</summary>
    public ReadOnlySpan<short> StageWeights(int stage) => _weights[stage];

    /// <summary>The mobility weights of one stage (48 own + 48 opponent slots), or empty when absent.</summary>
    public ReadOnlySpan<short> StageMobilityWeights(int stage) =>
        _mobilityWeights is null ? ReadOnlySpan<short>.Empty : _mobilityWeights[stage];

    /// <summary>The evaluation for the side to move, in 1/<see cref="UnitsPerDisc"/> discs.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public int Evaluate(ulong own, ulong opponent)
    {
        Span<int> indices = stackalloc int[Set.FeatureCount];
        Set.Indices(own, opponent, indices);
        int stage = _stageOfDiscs[System.Numerics.BitOperations.PopCount(own | opponent)];
        short[] weights = _weights[stage];
        int sum = 0;
        foreach (int index in indices)
        {
            sum += weights[index];
        }

        if (_mobilityWeights is { } mobility)
        {
            short[] mob = mobility[stage];
            int om = Math.Min(MobilitySlotsPerSide - 1, System.Numerics.BitOperations.PopCount(Bitboards.LegalMoves(own, opponent)));
            int pm = Math.Min(MobilitySlotsPerSide - 1, System.Numerics.BitOperations.PopCount(Bitboards.LegalMoves(opponent, own)));
            sum += mob[om] + mob[MobilitySlotsPerSide + pm];
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

    /// <summary>Reads an evaluation file, such as Stello's own, <c>Stello.Net/Book/stello-eval.bin</c>.</summary>
    /// <remarks>
    /// Stello's evaluation is built into the native C++ engine (Stello.Cpp), which searches with it unless
    /// <see cref="Search.SearchOptions.Pattern"/> gives another. It is not built into this assembly, so that the web
    /// app does not download it twice; the tools that evaluate in C# load the file, which their builds copy to their
    /// Data folder.
    /// </remarks>
    public static EvalWeights Load(string path)
    {
        byte[] bytes = File.ReadAllBytes(path);
        EvalWeights weights = Read(new MemoryStream(bytes, writable: false));
        weights._fileBytes = bytes;
        return weights;
    }

    // The file these weights were loaded from, or written once by FileBytes.
    private byte[]? _fileBytes;

    /// <summary>
    /// The weights as an evaluation file (see the remarks): the file they were loaded from, or else what
    /// <see cref="Write"/> writes. The native engine reads it (<c>EvalWeights::read</c>) to get the same weights.
    /// </summary>
    internal byte[] FileBytes
    {
        get
        {
            if (_fileBytes is null)
            {
                using var stream = new MemoryStream();
                Write(stream);
                _fileBytes = stream.ToArray();
            }

            return _fileBytes;
        }
    }

    /// <exception cref="InvalidDataException">The stream is not an evaluation file.</exception>
    public static EvalWeights Read(Stream stream)
    {
        using var gzip = new GZipStream(stream, CompressionMode.Decompress, leaveOpen: true);
        using var reader = new BinaryReader(gzip, Encoding.ASCII, leaveOpen: true);
        try
        {
            if (Encoding.ASCII.GetString(reader.ReadBytes(Magic.Length)) != Magic)
            {
                throw new InvalidDataException("Not a Stello evaluation file (version 2 or 3).");
            }

            int version = reader.ReadUInt16();
            if (version != Version && version != MobilityVersion)
            {
                throw new InvalidDataException("Not a Stello evaluation file (version 2 or 3).");
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

            short[][]? mobilityWeights = null;
            if (version == MobilityVersion)
            {
                mobilityWeights = new short[stages][];
                var mobPrev = new short[MobilityWeightsPerStage];
                for (int stage = 0; stage < stages; stage++)
                {
                    byte[] low = ReadExactly(reader, MobilityWeightsPerStage);
                    byte[] high = ReadExactly(reader, MobilityWeightsPerStage);
                    var stageMob = new short[MobilityWeightsPerStage];
                    for (int i = 0; i < MobilityWeightsPerStage; i++)
                    {
                        mobPrev[i] = (short)(mobPrev[i] + (short)(low[i] | (high[i] << 8)));
                        stageMob[i] = mobPrev[i];
                    }

                    mobilityWeights[stage] = stageMob;
                }
            }

            return new EvalWeights(set, stageOfDiscs, unitsPerDisc, weights, mobilityWeights);
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
        writer.Write((ushort)(HasMobility ? MobilityVersion : Version));
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
            WriteDeltaI16(writer, folded, previous);
            previous = folded;
        }

        if (_mobilityWeights is { } mobility)
        {
            short[]? mobPrev = null;
            for (int stage = 0; stage < Stages; stage++)
            {
                WriteDeltaI16(writer, mobility[stage], mobPrev);
                mobPrev = mobility[stage];
            }
        }
    }

    private static void WriteDeltaI16(BinaryWriter writer, short[] current, short[]? previous)
    {
        var low = new byte[current.Length];
        var high = new byte[current.Length];
        for (int i = 0; i < current.Length; i++)
        {
            short delta = (short)(current[i] - (previous?[i] ?? 0));
            low[i] = (byte)delta;
            high[i] = (byte)(delta >> 8);
        }

        writer.Write(low);
        writer.Write(high);
    }

    private static byte[] ReadExactly(BinaryReader reader, int count)
    {
        byte[] bytes = reader.ReadBytes(count);
        return bytes.Length == count ? bytes : throw new EndOfStreamException();
    }
}
