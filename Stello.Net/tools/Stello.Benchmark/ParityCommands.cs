using System.Buffers.Binary;
using System.Diagnostics;
using System.Globalization;
using System.Numerics;
using System.Text;
using Stello.BookTool;
using Stello.Engine;
using Stello.Engine.Benchmark;
using Stello.Engine.V2.Endgame;
using Stello.Engine.V2.Evaluation;

namespace Stello.Benchmark;

/// <summary>
/// parity-data: the fixtures that the Rust port of Stello 2.0 (Stello.Rust/stello-core) is checked against, and the
/// checksum of the evaluation over the trainer's test split (Plans/making-stello-3-plan.md, phase 1).
/// </summary>
internal static class ParityCommands
{
    private const ulong FnvOffset = 14695981039346656037UL;
    private const ulong FnvPrime = 1099511628211UL;

    // SearchEngine.PatternUnitsPerDisc in Stello.Engine.V2 (private there).
    private const int PatternUnitsPerDisc = 100;

    public static int ParityData(Options options)
    {
        options.Allow("out", "games", "seed", "split", "max-per-discs", "eval", "midgame", "count", "min-empties", "endgame", "max-empties", "pass-within");
        if (options.Text("endgame") is { } endgamePath)
        {
            int count = options.Number("count", 500, 1, 1_000_000);
            int minEmpties = options.Number("min-empties", 14, 1, 60);
            int maxEmpties = options.Number("max-empties", 24, minEmpties, 60);
            int passWithin = options.Number("pass-within", 0, 0, 60);
            WriteEndgamePositions(endgamePath, count, options.Number("seed", 1, 0, int.MaxValue), minEmpties, maxEmpties, passWithin);
            return 0;
        }

        if (options.Text("midgame") is { } midgamePath)
        {
            int count = options.Number("count", 1000, 1, 1_000_000);
            int minEmpties = options.Number("min-empties", 16, 1, 60);
            WriteMidgamePositions(midgamePath, count, options.Number("seed", 1, 0, int.MaxValue), minEmpties);
            return 0;
        }

        EvalWeights weights = EvalWeights.Load(options.Text("eval") ?? Program.DataFile("stello-eval.bin"));
        if (options.Text("split") is { } split)
        {
            string maxText = options.Text("max-per-discs") ?? "20000";
            long max = maxText == "all"
                ? long.MaxValue
                : long.TryParse(maxText, NumberStyles.None, CultureInfo.InvariantCulture, out long m) && m >= 1
                    ? m
                    : throw new ArgumentException("--max-per-discs must be a whole number from 1, or all.");
            EvalSplit(weights, split == "default" ? Path.Combine(Program.RepositoryFolder(), "Traning data", "stello-bin") : split, max);
            return 0;
        }

        string folder = options.Text("out")
            ?? Path.Combine(Program.RepositoryFolder(), "Stello.Rust", "stello-core", "tests", "data");
        int games = options.Number("games", 30, 0, 100_000);
        int seed = options.Number("seed", 1, 0, int.MaxValue);
        Directory.CreateDirectory(folder);

        List<(string Id, Board Board, Player Player)> positions = Positions(games, seed);
        WritePositions(Path.Combine(folder, "positions.txt"), positions, weights, games, seed);
        WritePatterns(Path.Combine(folder, "patterns.txt"), positions, weights);
        WritePerft(Path.Combine(folder, "perft.txt"));
        int passes = positions.Count(p => !p.Board.HasLegalMove(p.Player) && p.Board.HasLegalMove(p.Player.Opponent()));
        int over = positions.Count(p => p.Board.IsGameOver);
        Console.WriteLine($"Wrote {positions.Count:N0} positions ({passes} to pass, {over} finished games) to {folder}.");
        return 0;
    }

    /// <summary>
    /// The fixture positions: random games from fixed seeds (every position, passes and the final one included),
    /// mid-bench's and FFO #40–59's positions each followed by a random game to the end, and edge cases.
    /// </summary>
    private static List<(string Id, Board Board, Player Player)> Positions(int games, int seed)
    {
        var positions = new List<(string, Board, Player)>();
        for (int game = 0; game < games; game++)
        {
            AddRandomGame(positions, $"r{game}", Board.Initial, Player.Black, new Random(seed * 100_003 + game));
        }

        var fixedPositions = BenchmarkPosition.Load(Program.DataFile("midgame.txt"))
            .Concat(BenchmarkPosition.Load(Program.DataFile("ffo-40-59.txt")))
            .ToList();
        for (int i = 0; i < fixedPositions.Count; i++)
        {
            BenchmarkPosition position = fixedPositions[i];
            AddRandomGame(positions, $"p{position.Id}", position.Board, position.Player, new Random(seed * 7_919 + i));
        }

        ulong corners = 0x8100_0000_0000_0081UL;
        ulong edges = 0xFF81_8181_8181_81FFUL;
        void Edge(string id, ulong black, ulong white)
        {
            positions.Add(($"e-{id}-x", new Board(black, white), Player.Black));
            positions.Add(($"e-{id}-o", new Board(black, white), Player.White));
        }

        Edge("initial", Board.Initial.Black, Board.Initial.White);
        Edge("empty", 0, 0);
        Edge("one-disc", 1UL << 27, 0);
        Edge("full-black", ulong.MaxValue, 0);
        Edge("full-half", 0x0000_0000_FFFF_FFFFUL, 0xFFFF_FFFF_0000_0000UL);
        Edge("full-checker", 0x55AA_55AA_55AA_55AAUL, 0xAA55_AA55_AA55_AA55UL);
        Edge("wipeout", 0x0000_0018_1C00_0000UL, 0);
        Edge("corners", corners, 0x0000_0018_1800_0000UL);
        Edge("corners-mixed", 0x8000_0000_0000_0001UL | 0x0000_0010_0800_0000UL, 0x0100_0000_0000_0080UL | 0x0000_0008_1000_0000UL);
        Edge("edges-black", edges, ~edges & 0x0000_3C3C_3C3C_0000UL);
        Edge("edges-white", ~edges & 0x0000_3C3C_3C3C_0000UL, edges);
        Edge("edges-mixed", edges & 0x5555_5555_5555_5555UL, (edges & 0xAAAA_AAAA_AAAA_AAAAUL) | 0x0000_0018_1800_0000UL);
        // One empty square in each corner, the rest full: passes and one-move endings.
        Edge("last-corners", ~corners & 0x0F0F_0F0F_0F0F_0F0FUL, ~corners & 0xF0F0_F0F0_F0F0_F0F0UL);
        return positions;
    }

    /// <summary>
    /// Positions for mid-bench's parity runs of the Rust engine: from each of <paramref name="count"/> random games, one
    /// random position with at least <paramref name="minEmpties"/> empty squares and two legal moves. With 16 or more
    /// empty squares a search to depth 8 stays in the midgame (the solver takes over within 7 plies of the end).
    /// </summary>
    private static void WriteMidgamePositions(string path, int count, int seed, int minEmpties)
    {
        var random = new Random(seed);
        var lines = new List<string>
        {
            $"# Random midgame positions for parity runs (Stello.Benchmark parity-data --midgame --count {count} --seed {seed} --min-empties {minEmpties}):",
            $"# from each of {count} random games, one random position with at least {minEmpties} empty squares and two legal moves.",
        };
        var candidates = new List<(Board Board, Player Player)>();
        for (int game = 1; game <= count; game++)
        {
            candidates.Clear();
            Board board = Board.Initial;
            Player player = Player.Black;
            while (!board.IsGameOver && board.EmptyCount >= minEmpties)
            {
                ulong moves = board.LegalMoves(player);
                if (moves == 0)
                {
                    player = player.Opponent();
                    continue;
                }

                if (BitOperations.PopCount(moves) >= 2)
                {
                    candidates.Add((board, player));
                }

                Square[] squares = Square.InMask(moves).ToArray();
                board = board.Play(player, squares[random.Next(squares.Length)]);
                player = player.Opponent();
            }

            (Board chosen, Player side) = candidates[random.Next(candidates.Count)];
            lines.Add(new BenchmarkPosition($"r{game:0000}e{chosen.EmptyCount}", chosen, side, null, []).Format());
        }

        File.WriteAllLines(path, lines);
        Console.WriteLine($"Wrote {count:N0} positions to {path}.");
    }

    /// <summary>
    /// Positions for solve-bench's parity runs of the Rust engine's solver: from random games, one random position with
    /// <paramref name="minEmpties"/> to <paramref name="maxEmpties"/> empty squares and two legal moves, until there are
    /// <paramref name="count"/>. With <paramref name="passWithin"/> above 0, only positions where the random game has a
    /// pass within that many plies, so the solver meets passes in its upper layers too.
    /// </summary>
    private static void WriteEndgamePositions(string path, int count, int seed, int minEmpties, int maxEmpties, int passWithin)
    {
        var random = new Random(seed);
        var lines = new List<string>
        {
            $"# Random endgame positions for solve-bench parity runs (Stello.Benchmark parity-data --endgame --count {count} --seed {seed} " +
            $"--min-empties {minEmpties} --max-empties {maxEmpties} --pass-within {passWithin}):",
            $"# from random games, one random position with {minEmpties}-{maxEmpties} empty squares and two legal moves" +
            (passWithin > 0 ? $", and a pass in the game within {passWithin} plies." : "."),
        };
        var states = new List<(Board Board, Player Player, bool Pass)>();
        var candidates = new List<(Board Board, Player Player)>();
        for (int game = 1; lines.Count - 2 < count; game++)
        {
            states.Clear();
            Board board = Board.Initial;
            Player player = Player.Black;
            while (!board.IsGameOver)
            {
                ulong moves = board.LegalMoves(player);
                states.Add((board, player, moves == 0));
                if (moves == 0)
                {
                    player = player.Opponent();
                    continue;
                }

                Square[] squares = Square.InMask(moves).ToArray();
                board = board.Play(player, squares[random.Next(squares.Length)]);
                player = player.Opponent();
            }

            candidates.Clear();
            for (int i = 0; i < states.Count; i++)
            {
                (Board b, Player p, _) = states[i];
                if (b.EmptyCount < minEmpties || b.EmptyCount > maxEmpties || BitOperations.PopCount(b.LegalMoves(p)) < 2)
                {
                    continue;
                }

                if (passWithin > 0 && !states.Skip(i + 1).Take(passWithin).Any(s => s.Pass))
                {
                    continue;
                }

                candidates.Add((b, p));
            }

            if (candidates.Count == 0)
            {
                continue;
            }

            (Board chosen, Player side) = candidates[random.Next(candidates.Count)];
            lines.Add(new BenchmarkPosition($"r{game:00000}e{chosen.EmptyCount}", chosen, side, null, []).Format());
        }

        File.WriteAllLines(path, lines);
        Console.WriteLine($"Wrote {count:N0} positions to {path}.");
    }

    private static void AddRandomGame(List<(string, Board, Player)> positions, string id, Board board, Player player, Random random)
    {
        for (int ply = 0; ; ply++)
        {
            positions.Add(($"{id}.{ply}", board, player));
            ulong moves = board.LegalMoves(player);
            if (moves == 0)
            {
                if (!board.HasLegalMove(player.Opponent()))
                {
                    return;
                }

                player = player.Opponent();
                continue;
            }

            Square[] squares = Square.InMask(moves).ToArray();
            board = board.Play(player, squares[random.Next(squares.Length)]);
            player = player.Opponent();
        }
    }

    private static void WritePositions(string path, List<(string Id, Board Board, Player Player)> positions, EvalWeights weights, int games, int seed)
    {
        using var writer = new StreamWriter(path, false, new UTF8Encoding(false)) { NewLine = "\n" };
        writer.WriteLine($"# Parity fixture for Stello.Rust/stello-core, written by Stello.Benchmark parity-data --games {games} --seed {seed}");
        writer.WriteLine("# from Stello.Engine.V2 (C# Stello 2.0) and the shared Stello.Engine types. Do not edit; rewrite it with the command.");
        writer.WriteLine("# One position per line, bitboards in hex (bit n = square n, a1 = 0), own = the side to move:");
        writer.WriteLine("# id black white side(X|O) legal(own) legal(opponent) Bitboards.PotentialMobility(own,opp)");
        writer.WriteLine("#   EndgameBits.PotentialMobility(own,empty) EndgameBits.PotentialMobility(opp,empty) EndgameBits.LoneEmpties");
        writer.WriteLine("#   EndgameBits.Parity(empty) stage Evaluate(own,opp) PatternScore OrderingValue(depth 0)");
        writer.WriteLine("#   then for each legal move, lowest square first: square:flips:Evaluate(after the move, for the opponent).");
        writer.WriteLine("# The position after a move is the disc placed and the flips turned (checked here against Board.Play); the lines of one");
        writer.WriteLine("# game (id <game>.<ply>) follow each other, so each line is also the Board.Play result of a move or a pass in the line before.");
        writer.WriteLine($"# Evaluation: {weights.Set.Name}, {weights.Stages} stages, {weights.UnitsPerDisc} units per disc.");
        var line = new StringBuilder();
        foreach ((string id, Board board, Player player) in positions)
        {
            ulong own = board.Discs(player);
            ulong opponent = board.Discs(player.Opponent());
            ulong empty = board.Empty;
            ulong legal = board.LegalMoves(player);
            int eval = weights.Evaluate(own, opponent);
            line.Clear();
            line.Append(CultureInfo.InvariantCulture,
                $"{id} {Hex(board.Black)} {Hex(board.White)} {(player == Player.Black ? 'X' : 'O')} {Hex(legal)} {Hex(board.LegalMoves(player.Opponent()))} ");
            line.Append(CultureInfo.InvariantCulture,
                $"{Bitboards.PotentialMobility(own, opponent)} {EndgameBits.PotentialMobility(own, empty)} {EndgameBits.PotentialMobility(opponent, empty)} ");
            line.Append(CultureInfo.InvariantCulture,
                $"{Hex(EndgameBits.LoneEmpties(own, opponent))} {EndgameBits.Parity(empty)} {weights.StageOf(BitOperations.PopCount(own | opponent))} ");
            line.Append(CultureInfo.InvariantCulture,
                $"{eval} {PatternScore(weights, own, opponent)} {Math.Clamp(eval / weights.UnitsPerDisc, -64, 64)}");
            foreach (Square square in Square.InMask(legal))
            {
                ulong flips = board.Flips(player, square);
                Board after = board.Play(player, square);
                if (after != new Board(board.Black ^ (player == Player.Black ? flips | square.Bit : flips), board.White ^ (player == Player.White ? flips | square.Bit : flips)))
                {
                    throw new InvalidOperationException("Board.Play does not place the disc and turn the flips.");
                }

                int afterEval = weights.Evaluate(after.Discs(player.Opponent()), after.Discs(player));
                line.Append(CultureInfo.InvariantCulture, $" {square.Index}:{Hex(flips)}:{afterEval}");
            }

            writer.WriteLine(line);
        }
    }

    /// <summary>The pattern sets' shapes (sizes, placements, folds) and a checksum of the feature indices of every set.</summary>
    private static void WritePatterns(string path, List<(string Id, Board Board, Player Player)> positions, EvalWeights weights)
    {
        using var writer = new StreamWriter(path, false, new UTF8Encoding(false)) { NewLine = "\n" };
        writer.WriteLine("# Parity fixture for Stello.Rust/stello-core (Stello.Benchmark parity-data): the pattern sets of Stello.Engine.V2.");
        writer.WriteLine("# set name features raw-size symmetries; FNV-1a 64 of the features' indices (PatternSet.Indices, each int as 4 bytes");
        writer.WriteLine("#   little-endian) over all positions of positions.txt, for the side to move, in order");
        writer.WriteLine("# shape name size placements representatives FNV-1a(fold)");
        writer.WriteLine("# weights set stages units-per-disc stage-of-discs FNV-1a(all stages' weights by raw index, each short as 2 bytes)");
        foreach (PatternSet set in new[] { PatternSet.Classic11, PatternSet.Plus15, PatternSet.Egaroucid16 })
        {
            ulong hash = FnvOffset;
            var indices = new int[set.FeatureCount];
            foreach ((_, Board board, Player player) in positions)
            {
                set.Indices(board.Discs(player), board.Discs(player.Opponent()), indices);
                foreach (int index in indices)
                {
                    hash = Fnv(hash, index);
                }
            }

            int[] symmetries = Enumerable.Range(0, set.FeatureCount).Select(f => FeatureSymmetry(set, f)).ToArray();
            writer.WriteLine($"set {set.Name} {set.FeatureCount} {set.RawSize} {string.Join(',', symmetries)} {Hex(hash)}");
            foreach (PatternShape shape in set.Shapes)
            {
                ulong fold = FnvOffset;
                foreach (int f in shape.Fold)
                {
                    fold = Fnv(fold, f);
                }

                writer.WriteLine($"shape {shape.Name} {shape.Size} {string.Join(',', shape.Placements)} {shape.Representatives.Count} {Hex(fold)}");
            }
        }

        ulong all = FnvOffset;
        for (int stage = 0; stage < weights.Stages; stage++)
        {
            foreach (short weight in weights.StageWeights(stage))
            {
                all = Fnv16(all, weight);
            }
        }

        string stageOf = string.Join(',', Enumerable.Range(0, 65).Select(weights.StageOf));
        writer.WriteLine($"weights {weights.Set.Name} {weights.Stages} {weights.UnitsPerDisc} {stageOf} {Hex(all)}");
    }

    // The symmetry of each feature: the placements of the shapes in order (PatternSet's feature order).
    private static int FeatureSymmetry(PatternSet set, int feature)
    {
        foreach (PatternShape shape in set.Shapes)
        {
            if (feature < shape.Placements.Count)
            {
                return shape.Placements[feature];
            }

            feature -= shape.Placements.Count;
        }

        throw new ArgumentOutOfRangeException(nameof(feature));
    }

    private static void WritePerft(string path)
    {
        using var writer = new StreamWriter(path, false, new UTF8Encoding(false)) { NewLine = "\n" };
        writer.WriteLine("# Parity fixture for Stello.Rust/stello-core (Stello.Benchmark parity-data): perft counts from Stello.Engine's Board,");
        writer.WriteLine("# as Stello.Engine.Tests.PerftTests (a pass counts as one move, a finished game as one leaf).");
        writer.WriteLine("# id black white side depth leaves");
        for (int depth = 1; depth <= 8; depth++)
        {
            writer.WriteLine($"start {Hex(Board.Initial.Black)} {Hex(Board.Initial.White)} X {depth} {Perft(Board.Initial.Black, Board.Initial.White, depth)}");
        }

        foreach (BenchmarkPosition position in BenchmarkPosition.Load(Program.DataFile("midgame.txt")).Where((_, i) => i % 10 == 0)
            .Concat(BenchmarkPosition.Load(Program.DataFile("ffo-40-59.txt")).Take(4)))
        {
            ulong own = position.Board.Discs(position.Player);
            ulong opponent = position.Board.Discs(position.Player.Opponent());
            long leaves = Perft(own, opponent, 5);
            writer.WriteLine(
                $"{position.Id} {Hex(position.Board.Black)} {Hex(position.Board.White)} {(position.Player == Player.Black ? 'X' : 'O')} 5 {leaves}");
        }

        // Board.Play, as PerftTests uses it, must give the same counts as the raw bitboards.
        for (int depth = 1; depth <= 6; depth++)
        {
            if (BoardPerft(Board.Initial, Player.Black, depth) != Perft(Board.Initial.Black, Board.Initial.White, depth))
            {
                throw new InvalidOperationException($"Board.Play and Bitboards disagree at perft({depth}).");
            }
        }
    }

    /// <summary>Perft on raw bitboards with Stello.Engine's Bitboards (the search's move generation).</summary>
    public static long Perft(ulong own, ulong opponent, int depth)
    {
        if (depth == 0)
        {
            return 1;
        }

        ulong moves = Bitboards.LegalMoves(own, opponent);
        if (moves == 0)
        {
            return Bitboards.LegalMoves(opponent, own) != 0 ? Perft(opponent, own, depth - 1) : 1;
        }

        long leaves = 0;
        for (; moves != 0; moves &= moves - 1)
        {
            int square = BitOperations.TrailingZeroCount(moves);
            ulong flips = Bitboards.Flips(own, opponent, square);
            leaves += Perft(opponent & ~flips, own | flips | (1UL << square), depth - 1);
        }

        return leaves;
    }

    private static long BoardPerft(Board board, Player player, int depth)
    {
        if (depth == 0)
        {
            return 1;
        }

        ulong moves = board.LegalMoves(player);
        if (moves == 0)
        {
            return board.HasLegalMove(player.Opponent()) ? BoardPerft(board, player.Opponent(), depth - 1) : 1;
        }

        return Square.InMask(moves).Sum(move => BoardPerft(board.Play(player, move), player.Opponent(), depth - 1));
    }

    /// <summary>
    /// Evaluates the test split of the trainer's data (test-dNN.bin, 17-byte records: own u64, opponent u64, score sbyte)
    /// with at most <paramref name="maxPerDiscs"/> evenly spread positions per number of discs, as the trainer's report
    /// picks them, and prints the count and FNV-1a 64 checksums of the evaluations and PatternScores in order. Then times
    /// perft(8) and the evaluation, for the speed comparison with Rust.
    /// </summary>
    private static void EvalSplit(EvalWeights weights, string folder, long maxPerDiscs)
    {
        var samples = new List<(ulong Own, ulong Opponent)>();
        for (int discs = 4; discs <= 64; discs++)
        {
            string path = Path.Combine(folder, $"test-d{discs:00}.bin");
            if (!File.Exists(path))
            {
                continue;
            }

            byte[] bytes = File.ReadAllBytes(path);
            long count = bytes.Length / 17;
            long take = Math.Min(count, maxPerDiscs);
            for (long i = 0; i < take; i++)
            {
                long source = take == count ? i : i * count / take;
                ReadOnlySpan<byte> record = bytes.AsSpan((int)(source * 17), 17);
                samples.Add((BinaryPrimitives.ReadUInt64LittleEndian(record), BinaryPrimitives.ReadUInt64LittleEndian(record[8..])));
            }
        }

        if (samples.Count == 0)
        {
            throw new ArgumentException($"{folder} holds no test positions (test-dNN.bin).");
        }

        ulong evalHash = FnvOffset;
        ulong scoreHash = FnvOffset;
        foreach ((ulong own, ulong opponent) in samples)
        {
            evalHash = Fnv(evalHash, weights.Evaluate(own, opponent));
            scoreHash = Fnv(scoreHash, PatternScore(weights, own, opponent));
        }

        Console.WriteLine($"positions {samples.Count}");
        Console.WriteLine($"eval-fnv {Hex(evalHash)}");
        Console.WriteLine($"score-fnv {Hex(scoreHash)}");

        // Speed: the fastest of 3 passes over the positions, and of 3 perft(8) runs.
        double best = double.MaxValue;
        long sink = 0;
        for (int pass = 0; pass < 3; pass++)
        {
            var clock = Stopwatch.StartNew();
            foreach ((ulong own, ulong opponent) in samples)
            {
                sink += weights.Evaluate(own, opponent);
            }

            best = Math.Min(best, clock.Elapsed.TotalSeconds);
        }

        Console.WriteLine($"eval-speed {samples.Count / best / 1e6:F2} M/s ({best * 1000:F0} ms per pass, sum {sink})");
        best = double.MaxValue;
        long leaves = 0;
        for (int pass = 0; pass < 3; pass++)
        {
            var clock = Stopwatch.StartNew();
            leaves = Perft(Board.Initial.Black, Board.Initial.White, 8);
            best = Math.Min(best, clock.Elapsed.TotalSeconds);
        }

        Console.WriteLine($"perft8 {leaves} in {best * 1000:F1} ms");
    }

    // Stello.Engine.V2 SearchEngine.PatternScore (private there): the evaluation in 1/100 discs, capped at ±64 discs.
    private static int PatternScore(EvalWeights weights, ulong own, ulong opponent) =>
        Math.Clamp(
            weights.Evaluate(own, opponent) * PatternUnitsPerDisc / weights.UnitsPerDisc,
            -64 * PatternUnitsPerDisc,
            64 * PatternUnitsPerDisc);

    private static ulong Fnv(ulong hash, int value)
    {
        for (int i = 0; i < 4; i++)
        {
            hash = (hash ^ (byte)(value >> (8 * i))) * FnvPrime;
        }

        return hash;
    }

    private static ulong Fnv16(ulong hash, short value)
    {
        hash = (hash ^ (byte)value) * FnvPrime;
        return (hash ^ (byte)(value >> 8)) * FnvPrime;
    }

    private static string Hex(ulong value) => value.ToString("x", CultureInfo.InvariantCulture);
}
