using System.Diagnostics;
using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using Stello.BookTool;
using Stello.Engine;

namespace Stello.Trainer;

/// <summary>
/// convert: Egaroucid's training data (text) to <see cref="TrainingData"/> files. Two sources:
/// <list type="bullet">
/// <item><c>0001</c>: positions with the final score (or, up to 15 discs, a searched score), all kept.</item>
/// <item><c>v0002</c>: game records whose first N moves (the folder name) are random. The positions after the random
/// moves are kept, each with the game's final score, thinned so that each number of discs has about
/// <c>--cap</c> positions.</item>
/// </list>
/// Games (and, in 0001, lines) are split into training, validation and test by a hash, so the output does not depend on
/// the number of workers.
/// </summary>
internal static class ConvertCommand
{
    private const string Source1 = "Egaroucid_Train_Data/0001_egaroucid_7_5_1_lv17";
    private static readonly string[] Source2 =
    [
        "Egaroucid_Train_Data_v0002_0/0002_egaroucid_7_8_0_edax_4_5_5_lv11_0",
        "Egaroucid_Train_Data_v0002_1/0002_egaroucid_7_8_0_edax_4_5_5_lv11_1",
    ];

    private const int GamesPerFolder = 1_000_000;

    public static int Run(Options options)
    {
        options.Allow("data", "out", "cap", "workers", "max-files");
        string data = options.Text("data") ?? Path.Combine(TrainerProgram.RepositoryFolder(), "Traning data");
        string output = options.Text("out") ?? TrainingData.DefaultFolder();
        int cap = options.Number("cap", 10_000_000, 1000, 100_000_000);
        int workers = options.Number("workers", Environment.ProcessorCount, 1, 256);
        int maxFiles = options.Number("max-files", int.MaxValue, 1, int.MaxValue);

        List<Job> jobs = FindJobs(data, maxFiles);
        if (jobs.Count == 0)
        {
            throw new ArgumentException($"No training data found in {data} (give the folder with --data).");
        }

        Directory.CreateDirectory(output);
        Console.WriteLine($"convert: {jobs.Count:N0} files from {data} to {output}, about {cap:N0} game positions per number of discs, {workers} workers.");

        var writers = new Dictionary<(Split, int), BufferedStream>();
        var counts = new long[3, TrainingData.MaxDiscs + 1];
        var clock = Stopwatch.StartNew();
        long games = 0;
        long skipped = 0;
        try
        {
            for (int start = 0; start < jobs.Count; start += workers)
            {
                Job[] batch = jobs.Skip(start).Take(workers).ToArray();
                var results = new Result[batch.Length];
                Parallel.For(0, batch.Length, new ParallelOptions { MaxDegreeOfParallelism = workers },
                    i => results[i] = batch[i].RandomMoves < 0 ? ReadPositions(batch[i]) : ReadGames(batch[i], cap));

                // Written in the order of the files, so the output is the same for any number of workers.
                var buffer = new byte[Sample.Bytes];
                foreach (Result result in results)
                {
                    games += result.Games;
                    skipped += result.Skipped;
                    foreach ((Split split, Sample sample) in result.Samples)
                    {
                        int discs = sample.Discs;
                        if (!writers.TryGetValue((split, discs), out BufferedStream? writer))
                        {
                            writer = new BufferedStream(File.Create(Path.Combine(output, TrainingData.FileName(split, discs))), 1 << 20);
                            writers.Add((split, discs), writer);
                        }

                        sample.Write(buffer);
                        writer.Write(buffer);
                        counts[(int)split, discs]++;
                    }
                }

                Console.WriteLine($"  {Math.Min(start + workers, jobs.Count):N0}/{jobs.Count:N0} files, {games:N0} games, " +
                    $"{SumCounts(counts):N0} positions, {clock.Elapsed:h\\:mm\\:ss}");
            }
        }
        finally
        {
            foreach (BufferedStream writer in writers.Values)
            {
                writer.Dispose();
            }
        }

        string manifest = Manifest(output, counts, jobs.Count, games, skipped, cap);
        File.WriteAllText(Path.Combine(output, "manifest.txt"), manifest);
        Console.WriteLine(manifest);
        return 0;
    }

    private static long SumCounts(long[,] counts)
    {
        long sum = 0;
        foreach (long count in counts)
        {
            sum += count;
        }

        return sum;
    }

    /// <param name="RandomMoves">The number of random opening moves of a game file, or −1 for a file of positions.</param>
    private sealed record Job(string Path, int FileId, int RandomMoves);

    private sealed record Result(List<(Split, Sample)> Samples, long Games, long Skipped);

    private static List<Job> FindJobs(string data, int maxFiles)
    {
        var jobs = new List<Job>();
        string folder1 = Path.Combine(data, Source1);
        if (Directory.Exists(folder1))
        {
            jobs.AddRange(Directory.GetFiles(folder1, "*.txt").Order(StringComparer.Ordinal).Take(maxFiles)
                .Select(path => new Job(path, 0, -1)));
        }

        foreach (string source in Source2)
        {
            string folder = Path.Combine(data, source);
            if (!Directory.Exists(folder))
            {
                continue;
            }

            foreach (string moveFolder in Directory.GetDirectories(folder)
                .Where(d => int.TryParse(Path.GetFileName(d), CultureInfo.InvariantCulture, out _))
                .OrderBy(d => int.Parse(Path.GetFileName(d), CultureInfo.InvariantCulture)))
            {
                int randomMoves = int.Parse(Path.GetFileName(moveFolder), CultureInfo.InvariantCulture);
                jobs.AddRange(Directory.GetFiles(moveFolder, "*.txt").Order(StringComparer.Ordinal).Take(maxFiles)
                    .Select(path => new Job(path, 0, randomMoves)));
            }
        }

        return jobs.Select((job, i) => job with { FileId = i + 1 }).ToList();
    }

    /// <summary>Lines of 64 squares a1, b1, … h8 (X = side to move, O = opponent, - = empty), a space and the score.</summary>
    private static Result ReadPositions(Job job)
    {
        var samples = new List<(Split, Sample)>();
        long skipped = 0;
        long line = 0;
        foreach (string text in File.ReadLines(job.Path))
        {
            line++;
            if (text.Length < 66 || text[64] != ' '
                || !int.TryParse(text.AsSpan(65), NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out int score)
                || score is < -64 or > 64)
            {
                skipped++;
                continue;
            }

            ulong own = 0;
            ulong opponent = 0;
            for (int square = 0; square < 64; square++)
            {
                switch (text[square])
                {
                    case 'X':
                        own |= 1UL << square;
                        break;
                    case 'O':
                        opponent |= 1UL << square;
                        break;
                }
            }

            samples.Add((TrainingData.SplitOf(TrainingData.Hash((ulong)job.FileId, (ulong)line)), new Sample(own, opponent, (sbyte)score)));
        }

        return new Result(samples, 0, skipped);
    }

    /// <summary>Game records such as <c>f5d6c3…</c>, one per line, replayed with passes where needed.</summary>
    private static Result ReadGames(Job job, int cap)
    {
        var samples = new List<(Split, Sample)>();
        var positions = new List<(ulong Own, ulong Opponent, Player ToMove)>(60);
        long games = 0;
        long skipped = 0;
        long gameIndex = 0;
        foreach (string text in File.ReadLines(job.Path))
        {
            gameIndex++;
            string record = text.Trim();
            if (record.Length == 0)
            {
                continue;
            }

            positions.Clear();
            Board board = Board.Initial;
            Player toMove = Player.Black;
            bool valid = record.Length % 2 == 0;
            for (int ply = 0; valid && ply < record.Length / 2; ply++)
            {
                if (!board.HasLegalMove(toMove))
                {
                    toMove = toMove.Opponent();
                }

                if (!Square.TryParse(record.Substring(2 * ply, 2).ToLowerInvariant(), out Square? square) || !board.IsLegal(toMove, square.Value))
                {
                    valid = false;
                    break;
                }

                if (ply >= job.RandomMoves)
                {
                    positions.Add((board.Discs(toMove), board.Discs(toMove.Opponent()), toMove));
                }

                board = board.Play(toMove, square.Value);
                toMove = toMove.Opponent();
            }

            if (!valid || !board.IsGameOver)
            {
                skipped++;
                continue;
            }

            games++;
            Split split = TrainingData.SplitOf(TrainingData.Hash((ulong)job.FileId, (ulong)gameIndex));
            int black = FinalScore(board);
            foreach ((ulong own, ulong opponent, Player player) in positions)
            {
                // Positions with d discs come from the folders with at most d − 4 random moves (from 8 on).
                int discs = System.Numerics.BitOperations.PopCount(own | opponent);
                double available = Math.Max(1, discs - 4 - 8 + 1) * (double)GamesPerFolder;
                double keep = Math.Min(1, cap / available);
                if (keep < 1 && TrainingData.Hash((ulong)job.FileId, (ulong)gameIndex, (ulong)discs) >= keep * ulong.MaxValue)
                {
                    continue;
                }

                samples.Add((split, new Sample(own, opponent, (sbyte)(player == Player.Black ? black : -black))));
            }
        }

        return new Result(samples, games, skipped);
    }

    /// <summary>Black's discs minus White's at the end, with the empty squares given to the winner.</summary>
    private static int FinalScore(Board board)
    {
        int black = board.Count(Player.Black);
        int white = board.Count(Player.White);
        int empty = 64 - black - white;
        return black > white ? black - white + empty : black < white ? black - white - empty : 0;
    }

    private static string Manifest(string output, long[,] counts, int files, long games, long skipped, int cap)
    {
        var text = new StringBuilder();
        text.AppendLine(CultureInfo.InvariantCulture, $"# Stello training data, converted from Egaroucid's training data ({DateTime.Now:yyyy-MM-dd HH:mm}).");
        text.AppendLine("# Do not commit or redistribute: the terms of the source data forbid it.");
        text.AppendLine(CultureInfo.InvariantCulture, $"# {files:N0} files, {games:N0} games replayed, {skipped:N0} lines skipped, cap {cap:N0} per number of discs.");
        text.AppendLine("# file positions sha256");
        using var hashAll = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        foreach (Split split in Enum.GetValues<Split>())
        {
            for (int discs = TrainingData.MinDiscs; discs <= TrainingData.MaxDiscs; discs++)
            {
                if (counts[(int)split, discs] == 0)
                {
                    continue;
                }

                string name = TrainingData.FileName(split, discs);
                byte[] hash = SHA256.HashData(File.ReadAllBytes(Path.Combine(output, name)));
                hashAll.AppendData(hash);
                text.AppendLine(CultureInfo.InvariantCulture, $"{name} {counts[(int)split, discs]} {Convert.ToHexStringLower(hash)}");
            }
        }

        text.AppendLine(CultureInfo.InvariantCulture, $"# all {Convert.ToHexStringLower(hashAll.GetHashAndReset())}");
        return text.ToString();
    }
}
