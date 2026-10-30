using System.Diagnostics;
using System.Numerics;
using System.Text.Json;
using Stello.BookTool;
using Stello.Engine;
using Stello.Engine.Benchmark;

namespace Stello.Benchmark;

/// <summary>One search of a benchmark: what it found and what it cost. Time is the fastest of the passes.</summary>
internal sealed record BenchResult(string Id, int Empties, string Move, int Score, string Kind, int Depth, long Nodes, double Ms);

/// <summary>A benchmark run as saved by --out and read by --compare.</summary>
internal sealed record BenchRun(string Command, string Date, int ProcessorCount, string Limits, int HashBits, int Passes, IReadOnlyList<BenchResult> Results)
{
    /// <summary>stello or egaroucid; runs saved before the Egaroucid port have no engine and are Stello's.</summary>
    public string Engine { get; init; } = "stello";
}

/// <summary>The speed benchmarks: solve-bench, mid-bench, and make-positions for the mid-bench positions.</summary>
internal static class BenchCommands
{
    private static readonly JsonSerializerOptions Json = new() { WriteIndented = true };

    public static int SolveBench(Options options)
    {
        options.Allow("positions", "from", "to", "passes", "hash-bits", "out", "compare", "engine");
        List<BenchmarkPosition> all = BenchmarkPosition.Load(options.Text("positions") ?? Program.DataFile("ffo-40-59.txt"));
        int from = options.Number("from", 40, 0, int.MaxValue);
        int to = options.Number("to", 44, 0, int.MaxValue);
        List<BenchmarkPosition> positions = all
            .Where(p => !int.TryParse(p.Id, out int id) || (id >= from && id <= to))
            .ToList();

        BenchRun run = Run("solve-bench", positions, BenchSearcher.Create(options, SearchLimits.Solve, "solve"), options);
        int wrong = 0;
        Console.WriteLine();
        Console.WriteLine("| # | Empty | Score | Move | Nodes | Time | Nodes/s |");
        Console.WriteLine("|---|---:|---:|---|---:|---:|---:|");
        foreach ((BenchmarkPosition position, BenchResult result) in positions.Zip(run.Results))
        {
            bool scoreOk = position.Score is null || (result.Kind == nameof(ScoreKind.Exact) && result.Score == position.Score);
            bool moveOk = position.BestMoves.Count == 0 || position.BestMoves.Any(m => m.ToString() == result.Move);
            if (!scoreOk || !moveOk)
            {
                wrong++;
            }

            Console.WriteLine(
                $"| {result.Id} | {result.Empties} | {result.Score:+0;-0;0}{(scoreOk ? "" : $" (should be {position.Score:+0;-0;0})")} | " +
                $"{result.Move}{(moveOk ? "" : " (wrong)")} | {result.Nodes:N0} | {Seconds(result.Ms)} | {Speed(result.Nodes, result.Ms)} |");
        }

        WriteTotal(run, "| **Total** | | | | ");
        Console.WriteLine(wrong == 0 ? "All scores and moves are correct." : $"{wrong} positions have a wrong score or move.");
        Finish(run, options);
        return wrong == 0 ? 0 : 3;
    }

    public static int MidBench(Options options)
    {
        options.Allow("positions", "depth", "passes", "hash-bits", "out", "compare", "engine", "mpc", "level");
        List<BenchmarkPosition> positions = BenchmarkPosition.Load(options.Text("positions") ?? Program.DataFile("midgame.txt"));
        int depth = options.Number("depth", 10, 1, 60);

        BenchRun run = Run("mid-bench", positions, BenchSearcher.Create(options, SearchLimits.FixedDepth(depth), $"depth {depth}"), options);
        Console.WriteLine();
        Console.WriteLine("| Empty squares | Positions | Nodes | Time | Nodes/s |");
        Console.WriteLine("|---:|---:|---:|---:|---:|");
        foreach (IGrouping<int, BenchResult> group in run.Results.GroupBy(r => r.Empties).OrderByDescending(g => g.Key))
        {
            long nodes = group.Sum(r => r.Nodes);
            double ms = group.Sum(r => r.Ms);
            Console.WriteLine($"| {group.Key} | {group.Count()} | {nodes:N0} | {Seconds(ms)} | {Speed(nodes, ms)} |");
        }

        WriteTotal(run, $"| **Total** | {run.Results.Count} | ");
        Finish(run, options);
        return 0;
    }

    public static int MakePositions(Options options)
    {
        options.Allow("starts", "games", "depth", "empties", "out");
        List<string> starts = MatchCommands.LoadStarts(options.Text("starts"));
        int games = options.Number("games", 20, 1, starts.Count);
        int depth = options.Number("depth", 6, 1, 20);
        int[] empties = NumberList(options, "empties", "48,42,36,30,26", 1, 59);
        string path = options.Text("out") ?? Program.DataFile("midgame.txt");

        var lines = new List<string>
        {
            $"# Midgame positions for mid-bench: from the first {games} start positions in starts.txt, the engine played itself at",
            $"# depth {depth} without the book; a position was taken when {string.Join(", ", empties)} squares were empty",
            "# (or the first position after that with at least two legal moves).",
        };
        lines.AddRange(SelfPlayPositions(starts, 0, games, depth, empties).Select(p => p.Format()));

        File.WriteAllLines(path, lines);
        Console.WriteLine($"Wrote {lines.Count(l => !l.StartsWith('#'))} positions to {path}.");
        return 0;
    }

    /// <summary>A comma-separated list of whole numbers from <paramref name="min"/> to <paramref name="max"/>.</summary>
    /// <exception cref="ArgumentException">The option is not such a list.</exception>
    public static int[] NumberList(Options options, string name, string defaultValue, int min, int max) =>
        (options.Text(name) ?? defaultValue)
            .Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries)
            .Select(text => int.TryParse(text, out int n) && n >= min && n <= max ? n : throw new ArgumentException($"--{name} must be numbers from {min} to {max}."))
            .ToArray();

    /// <summary>
    /// Positions from games Stello plays against itself at <paramref name="depth"/> without the book, from the start
    /// positions <paramref name="first"/> to <paramref name="first"/> + <paramref name="games"/> − 1: one when each of
    /// <paramref name="empties"/> squares are empty, or the first position after that with at least two legal moves.
    /// The ids are g&lt;game&gt;e&lt;empties&gt;, the game counted from 1.
    /// </summary>
    public static List<BenchmarkPosition> SelfPlayPositions(List<string> starts, int first, int games, int depth, int[] empties)
    {
        var engine = new SearchEngine(16);
        var positions = new List<BenchmarkPosition>();
        for (int game = first; game < first + games; game++)
        {
            (Board board, Player player) = BookTextFormat.Replay(starts[game]);
            var wanted = new Queue<int>(empties.OrderDescending());
            while (!board.IsGameOver && wanted.Count > 0)
            {
                if (!board.HasLegalMove(player))
                {
                    player = player.Opponent();
                    continue;
                }

                if (board.EmptyCount <= wanted.Peek() && BitOperations.PopCount(board.LegalMoves(player)) >= 2)
                {
                    positions.Add(new BenchmarkPosition($"g{game + 1:00}e{wanted.Dequeue()}", board, player, null, []));
                    while (wanted.Count > 0 && wanted.Peek() >= board.EmptyCount)
                    {
                        wanted.Dequeue();
                    }
                }

                engine.ClearHash();
                Square move = engine.Search(board, player, SearchLimits.FixedDepth(depth)).Move.Square!.Value;
                board = board.Play(player, move);
                player = player.Opponent();
            }
        }

        return positions;
    }

    // Interleaved passes (every position once, then again), so a slow moment of the machine does not hit one position
    // in every pass; the fastest time of each position counts. Each search starts with empty hash tables.
    private static BenchRun Run(string command, List<BenchmarkPosition> positions, BenchSearcher searcher, Options options)
    {
        if (positions.Count == 0)
        {
            throw new ArgumentException("No positions to search.");
        }

        int passes = options.Number("passes", command == "solve-bench" ? 1 : 3, 1, 100);
        var best = new BenchResult[positions.Count];
        Console.WriteLine($"{command}: {positions.Count} positions, {searcher.Description}, {passes} pass(es).");

        for (int pass = 1; pass <= passes; pass++)
        {
            var clock = Stopwatch.StartNew();
            long nodes = 0;
            for (int i = 0; i < positions.Count; i++)
            {
                BenchmarkPosition position = positions[i];
                SearchResult result = searcher.Search(position);
                var current = new BenchResult(
                    position.Id, position.Board.EmptyCount, result.Move.ToString(), result.Score, result.Kind.ToString(),
                    result.Depth, result.Nodes, result.Elapsed.TotalMilliseconds);
                nodes += result.Nodes;

                if (best[i] is { } previous && (previous.Nodes != current.Nodes || previous.Move != current.Move || previous.Score != current.Score))
                {
                    Console.WriteLine($"  Warning: {position.Id} gave another result in pass {pass} ({current.Nodes:N0} nodes, {current.Move}).");
                }

                best[i] = best[i] is { } old && old.Ms <= current.Ms ? old : current;
                if (command == "solve-bench")
                {
                    Console.WriteLine($"  pass {pass} #{position.Id}: {current.Score:+0;-0;0} {current.Move} in {Seconds(current.Ms)} ({current.Nodes:N0} nodes)");
                }
            }

            Console.WriteLine($"  pass {pass}: {nodes:N0} nodes in {clock.Elapsed.TotalSeconds:0.00} s");
        }

        return new BenchRun(command, DateTime.Now.ToString("yyyy-MM-dd HH:mm"), Environment.ProcessorCount, searcher.Limits, searcher.HashBits, passes, best)
        {
            Engine = searcher.Engine,
        };
    }

    private static void WriteTotal(BenchRun run, string firstColumns)
    {
        long nodes = run.Results.Sum(r => r.Nodes);
        double ms = run.Results.Sum(r => r.Ms);
        Console.WriteLine($"{firstColumns}**{nodes:N0}** | **{Seconds(ms)}** | **{Speed(nodes, ms)}** |");
    }

    private static void Finish(BenchRun run, Options options)
    {
        if (options.Text("compare") is { } comparePath)
        {
            Compare(run, JsonSerializer.Deserialize<BenchRun>(File.ReadAllText(comparePath))
                ?? throw new InvalidDataException($"{comparePath} holds no benchmark run."), comparePath);
        }

        if (options.Text("out") is { } outPath)
        {
            File.WriteAllText(outPath, JsonSerializer.Serialize(run, Json));
            Console.WriteLine($"Saved to {outPath}.");
        }
    }

    private static void Compare(BenchRun run, BenchRun saved, string path)
    {
        Dictionary<string, BenchResult> old = saved.Results.ToDictionary(r => r.Id);
        var differing = new List<string>();
        int compared = 0;
        double ms = 0;
        double oldMs = 0;
        foreach (BenchResult result in run.Results)
        {
            if (!old.TryGetValue(result.Id, out BenchResult? before))
            {
                continue;
            }

            compared++;
            ms += result.Ms;
            oldMs += before.Ms;
            if (before.Nodes != result.Nodes || before.Move != result.Move || before.Score != result.Score)
            {
                differing.Add($"{result.Id}: {before.Nodes:N0} -> {result.Nodes:N0} nodes, {before.Move} -> {result.Move}, {before.Score} -> {result.Score}");
            }
        }

        Console.WriteLine();
        Console.WriteLine($"Against {path} ({saved.Date}, {saved.Limits}):");
        Console.WriteLine(differing.Count == 0
            ? $"  identical: all {compared} searches visited the same nodes and found the same move and score."
            : $"  {compared - differing.Count} of {compared} searches identical; these differ:");
        foreach (string line in differing.Take(20))
        {
            Console.WriteLine($"    {line}");
        }

        if (oldMs > 0)
        {
            Console.WriteLine($"  time {Seconds(ms)} against {Seconds(oldMs)} ({(oldMs / ms - 1) * 100:+0.0;-0.0;0.0} % speed).");
        }
    }

    private static string Seconds(double ms) => ms >= 10_000 ? $"{ms / 1000:0.0} s" : $"{ms / 1000:0.000} s";

    private static string Speed(long nodes, double ms) => ms > 0 ? $"{nodes / ms / 1000:0.00}M" : "-";
}
