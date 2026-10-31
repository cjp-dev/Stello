using System.Buffers.Binary;
using System.Numerics;

namespace Stello.Trainer;

/// <summary>One training position: the discs of the side to move and of the opponent, and the final score.</summary>
/// <param name="Score">The disc difference at the end of the game for the side to move (empty squares to the winner).</param>
internal readonly record struct Sample(ulong Own, ulong Opponent, sbyte Score)
{
    public const int Bytes = 17;

    public int Discs => BitOperations.PopCount(Own | Opponent);

    public void Write(Span<byte> destination)
    {
        BinaryPrimitives.WriteUInt64LittleEndian(destination, Own);
        BinaryPrimitives.WriteUInt64LittleEndian(destination[8..], Opponent);
        destination[16] = (byte)Score;
    }

    public static Sample Read(ReadOnlySpan<byte> source) => new(
        BinaryPrimitives.ReadUInt64LittleEndian(source),
        BinaryPrimitives.ReadUInt64LittleEndian(source[8..]),
        (sbyte)source[16]);
}

/// <summary>Which part of the data a game belongs to.</summary>
internal enum Split
{
    Train,
    Validation,
    Test,
}

/// <summary>
/// The converted training data: one file of <see cref="Sample"/>s per split and number of discs, such as
/// <c>train-d20.bin</c>. It is made from Egaroucid's training data and must not be redistributed or committed.
/// </summary>
internal static class TrainingData
{
    public const int MinDiscs = 4;
    public const int MaxDiscs = 64;

    public static string DefaultFolder() => Path.Combine(TrainerProgram.RepositoryFolder(), "Traning data", "stello-bin");

    public static string FileName(Split split, int discs) => $"{split.ToString().ToLowerInvariant()}-d{discs:00}.bin";

    public static long Count(string folder, Split split, int discs)
    {
        var file = new FileInfo(Path.Combine(folder, FileName(split, discs)));
        return file.Exists ? file.Length / Sample.Bytes : 0;
    }

    /// <summary>
    /// The samples of one split and number of discs. With <paramref name="max"/>, an evenly spread subset (every k-th
    /// sample), because the files are in the order of the source files.
    /// </summary>
    public static Sample[] Load(string folder, Split split, int discs, long max = long.MaxValue)
    {
        string path = Path.Combine(folder, FileName(split, discs));
        if (!File.Exists(path))
        {
            return [];
        }

        byte[] bytes = File.ReadAllBytes(path);
        long count = bytes.Length / Sample.Bytes;
        long take = Math.Min(count, max);
        var samples = new Sample[take];
        for (long i = 0; i < take; i++)
        {
            long source = take == count ? i : i * count / take;
            samples[i] = Sample.Read(bytes.AsSpan((int)(source * Sample.Bytes), Sample.Bytes));
        }

        return samples;
    }

    /// <summary>A deterministic hash of a few numbers, for splitting and sampling (SplitMix64 steps).</summary>
    public static ulong Hash(ulong a, ulong b = 0, ulong c = 0)
    {
        ulong x = Mix(a ^ 0x9E37_79B9_7F4A_7C15UL);
        x = Mix(x ^ b);
        return Mix(x ^ c);
    }

    /// <summary>The split of a game: 2 % validation, 2 % test, the rest training.</summary>
    public static Split SplitOf(ulong gameHash) => (gameHash % 50) switch
    {
        0 => Split.Validation,
        1 => Split.Test,
        _ => Split.Train,
    };

    private static ulong Mix(ulong x)
    {
        x += 0x9E37_79B9_7F4A_7C15UL;
        x = (x ^ (x >> 30)) * 0xBF58_476D_1CE4_E5B9UL;
        x = (x ^ (x >> 27)) * 0x94D0_49BB_1331_11EBUL;
        return x ^ (x >> 31);
    }
}
