// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/hash.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later.

namespace Stello.Egaroucid;

/// <summary>
/// The random numbers behind <see cref="Board.Hash"/> (hash.hpp): 4 × 65536 numbers for the side to move and 4 × 65536
/// for the opponent, read from <c>resources/hash/hash&lt;level&gt;.eghs</c>. Egaroucid ships them for levels 25-27 only; other
/// levels would use clock-seeded random numbers, so the port supports only those.
/// </summary>
internal static class HashTables
{
    public const int DefaultHashLevel = 25;
    public const int MinHashLevel = 25;
    public const int MaxHashLevel = 27;

    /// <summary>[i * 65536 + 16 bits of the side to move's discs, from the lowest 16 bits up].</summary>
    public static uint[] RandPlayer { get; private set; } = new uint[4 * 65536];

    public static uint[] RandOpponent { get; private set; } = new uint[4 * 65536];

    public static int Level { get; private set; }

    /// <exception cref="ArgumentOutOfRangeException"><paramref name="hashLevel"/> is not 25, 26 or 27.</exception>
    /// <exception cref="IOException">The file is missing or too short.</exception>
    public static void Load(string resourceDirectory, int hashLevel)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(hashLevel, MinHashLevel);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(hashLevel, MaxHashLevel);
        string path = Path.Combine(resourceDirectory, "hash", $"hash{hashLevel}.eghs");
        byte[] bytes = File.ReadAllBytes(path);
        if (bytes.Length < 8 * 65536 * 4)
        {
            throw new IOException($"{path} is too short");
        }

        var player = new uint[4 * 65536];
        var opponent = new uint[4 * 65536];
        Buffer.BlockCopy(bytes, 0, player, 0, player.Length * 4);
        Buffer.BlockCopy(bytes, player.Length * 4, opponent, 0, opponent.Length * 4);
        if (!BitConverter.IsLittleEndian)
        {
            throw new PlatformNotSupportedException("The hash files are little-endian");
        }

        RandPlayer = player;
        RandOpponent = opponent;
        Level = hashLevel;
    }
}
