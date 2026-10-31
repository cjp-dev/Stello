// SPDX-License-Identifier: GPL-3.0-only

using System.Diagnostics;
using System.Globalization;
using System.Text;
using System.Text.Json;
using Stello.BookTool;
using Stello.Egaroucid;
using Stello.Engine;
using Stello.Engine.Benchmark;
using Board = Stello.Engine.Board;
using SearchResult = Stello.Engine.SearchResult;

namespace Stello.Benchmark;

/// <summary>One engine's moves at one time limit: how many discs it lost against perfect play.</summary>
/// <param name="AverageLoss">The average of (best score − score of the chosen move), in discs.</param>
/// <param name="LossMargin">The half-width of the 95 % interval of <paramref name="AverageLoss"/>.</param>
/// <param name="BestMoveRate">The share of positions where the chosen move was one of the best.</param>
/// <param name="SolvedRate">The share of searches that ended with an exact or win/loss/draw score.</param>
/// <param name="AverageMs">The average wall time of a move.</param>
internal sealed record QualityResult(string Engine, int TimeMs, string Empties, int Positions, double AverageLoss, double LossMargin, double BestMoveRate,
    double SolvedRate, double AverageMs);

/// <param name="Difference">The first engine's average loss minus the second's, position by position.</param>
internal sealed record QualityDifference(int TimeMs, string First, string Second, double Difference, double Margin);

internal sealed record QualityRun(string Date, string Positions, int Workers, IReadOnlyList<QualityResult> Results, IReadOnlyList<QualityDifference> Differences);

/// <summary>
/// make-exact and mid-quality: what each engine's move is worth in positions whose every move has been solved exactly,
/// at fixed time per move. This measures the midgame search as a whole, apart from opening choices and style.
/// </summary>
internal static class QualityCommands
{
    private static readonly JsonSerializerOptions Json = new() { WriteIndented = true };

    public static int MakeExact(Options options)
    {
        options.Allow("starts", "from", "games", "depth", "empties", "workers", "hash-bits", "out");
        List<string> starts = MatchCommands.LoadStarts(options.Text("starts"));
        int from = options.Number("from", 20, 0, starts.Count - 1);
        int games = options.Number("games", 100, 1, starts.Count - from);
        int depth = options.Number("depth", 6, 1, 20);
        int[] empties = BenchCommands.NumberList(options, "empties", "28,26,24", 1, 40);
        int workers = options.Number("workers", Math.Max(1, Environment.ProcessorCount / 2), 1, 256);
        int tableBits = options.Number("hash-bits", 23, 10, HashTables.DefaultHashLevel);
        string path = options.Text("out") ?? Program.DataFile("midgame-exact.txt");

        MatchCommands.LoadEgaroucid();
        List<BenchmarkPosition> positions = BenchCommands.SelfPlayPositions(starts, from, games, depth, empties);
        var solved = new BenchmarkPosition?[positions.Count];
        if (File.Exists(path))
        {
            Dictionary<string, BenchmarkPosition> saved = BenchmarkPosition.Load(path).Where(p => p.MoveScores.Count > 0).ToDictionary(p => p.Id);
            for (int i = 0; i < positions.Count; i++)
            {
                if (saved.TryGetValue(positions[i].Id, out BenchmarkPosition? old) && old.Board == positions[i].Board && old.Player == positions[i].Player)
                {
                    solved[i] = old;
                }
            }
        }

        string[] header =
        [
            $"# Positions for mid-quality: from start positions {from + 1} to {from + games} in starts.txt, Stello played itself at depth {depth}",
            $"# without the book; a position was taken when {string.Join(", ", empties)} squares were empty (or the first position after that",
            "# with at least two legal moves). Every legal move is scored exactly by the Egaroucid port (level 60), best first.",
        ];

        int done = solved.Count(p => p is not null);
        Console.WriteLine($"make-exact: {positions.Count} positions, {done} already solved in {path}; {workers} at a time.");
        var gate = new object();
        var clock = Stopwatch.StartNew();
        int next = -1;
        Program.Cancellable(token => ParallelWork.Run(workers, () =>
        {
            var context = new SearchContext(tableBits);
            int index;
            while ((index = Interlocked.Increment(ref next)) < positions.Count)
            {
                if (solved[index] is not null)
                {
                    continue;
                }

                BenchmarkPosition position = positions[index];
                var scores = new List<(Square Move, int Score)>();
                foreach (Square move in Square.InMask(position.Board.LegalMoves(position.Player)))
                {
                    token.ThrowIfCancellationRequested();
                    scores.Add((move, ExactScore(context, position.Board.Play(position.Player, move), position.Player.Opponent())));
                }

                BenchmarkPosition result = position with { Score = scores.Max(s => s.Score) };
                result = result with { MoveScores = scores.OrderByDescending(s => s.Score).ThenBy(s => s.Move.Index).ToList() };
                lock (gate)
                {
                    solved[index] = result;
                    done++;
                    Console.WriteLine($"  [{done}/{positions.Count}] {result.Id}: {result.Score:+0;-0;0}, {scores.Count} moves, {Program.Duration(clock.Elapsed)}");
                    File.WriteAllLines(path, header.Concat(solved.OfType<BenchmarkPosition>().Select(p => p.Format())));
                }
            }
        }, token));

        Console.WriteLine($"Wrote {solved.Count(p => p is not null)} positions to {path}.");
        return solved.All(p => p is not null) ? 0 : 2;
    }

    /// <summary>
    /// The exact score for the player who just moved, after the move: the position <paramref name="board"/> with
    /// <paramref name="toMove"/> to move is solved, passing first if needed (Egaroucid's search does not pass at the root).
    /// </summary>
    private static int ExactScore(SearchContext context, Board board, Player toMove)
    {
        context.Clear();
        if (!board.HasLegalMove(toMove) && board.HasLegalMove(toMove.Opponent()))
        {
            return Ai.Search(context, EgaroucidPlayer.ToEgaroucid(board, toMove.Opponent()), Level.MAX_LEVEL, useMultiThread: false).Value;
        }

        // For a finished game, ai gives the final score without searching.
        return -Ai.Search(context, EgaroucidPlayer.ToEgaroucid(board, toMove), Level.MAX_LEVEL, useMultiThread: false).Value;
    }

    public static int MidQuality(Options options)
    {
        options.Allow("positions", "times", "engines", "sides", "workers", "max-positions", "name");
        string path = options.Text("positions") ?? Program.DataFile("midgame-exact.txt");
        List<BenchmarkPosition> positions = BenchmarkPosition.Load(path).Where(p => p.MoveScores.Count > 0).ToList();
        positions = positions.Take(options.Number("max-positions", positions.Count, 1, int.MaxValue)).ToList();
        int[] times = BenchCommands.NumberList(options, "times", "50,200,1000", 1, 600_000);
        // --sides: match sides separated by ';' (their time is replaced), named by their name= key; --engines: plain engines.
        string[] sideTexts = options.Text("sides") is { } sidesText
            ? sidesText.Split(';', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries)
            : (options.Text("engines") ?? "stello,egaroucid").Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries)
                .Select(engine => engine is "stello" or "stello-v1" or "egaroucid"
                    ? $"engine={engine},name={engine}"
                    : throw new ArgumentException("--engines must be a list of stello, stello-v1 and egaroucid."))
                .ToArray();
        int workers = options.Number("workers", Math.Max(1, Environment.ProcessorCount / 2), 1, 256);
        string name = options.Text("name") ?? "mid-quality";
        if (positions.Count == 0)
        {
            throw new ArgumentException($"{path} holds no positions with move scores (make them with make-exact).");
        }

        MatchSide[] sides = sideTexts.Select(text => MatchCommands.ParseSide(text + ",time-ms=1")).ToArray();
        string[] engines = sides.Select(side => side.Name).ToArray();
        if (engines.Distinct().Count() != engines.Length)
        {
            throw new ArgumentException("Give every side of mid-quality its own name= .");
        }

        Console.WriteLine($"mid-quality: {positions.Count} positions, {string.Join(", ", times)} ms per move, {string.Join(" and ", engines)}, {workers} at a time.");
        foreach (MatchSide side in sides)
        {
            Console.WriteLine($"  {MatchCommands.Describe(side)}");
        }

        // Every task is one position at one time; both engines search it one after the other in the same worker, so a
        // busy moment of the machine hits both alike.
        var tasks = times.SelectMany(time => positions.Select((_, i) => (Time: time, Index: i))).ToArray();
        var moves = new (double Loss, bool Solved, double Ms)[engines.Length, tasks.Length];
        int next = -1;
        int done = 0;
        var clock = Stopwatch.StartNew();
        bool completed = Program.Cancellable(token => ParallelWork.Run(workers, () =>
        {
            IMatchPlayer[] players = sides.Select(side => side.NewPlayer()).ToArray();
            int task;
            while ((task = Interlocked.Increment(ref next)) < tasks.Length)
            {
                (int time, int index) = tasks[task];
                BenchmarkPosition position = positions[index];
                SearchLimits limits = SearchLimits.TimePerMove(TimeSpan.FromMilliseconds(time), useOpeningBook: false);
                for (int e = 0; e < players.Length; e++)
                {
                    players[e].NewGame();
                    long started = Stopwatch.GetTimestamp();
                    SearchResult result = players[e].ChooseMove(position.Board, position.Player, limits, token);
                    double ms = Stopwatch.GetElapsedTime(started).TotalMilliseconds;
                    int score = position.MoveScores.First(m => m.Move == result.Move.Square).Score;
                    moves[e, task] = (position.Score!.Value - score, result.Kind is ScoreKind.Exact or ScoreKind.WinLossDraw, ms);
                }

                if (Interlocked.Increment(ref done) % 100 == 0)
                {
                    Console.WriteLine($"  [{done}/{tasks.Length}] {Program.Duration(clock.Elapsed)}");
                }
            }
        }, token));

        if (!completed)
        {
            Console.WriteLine("Stopped; nothing saved.");
            return 2;
        }

        var results = new List<QualityResult>();
        var differences = new List<QualityDifference>();
        foreach (int time in times)
        {
            int[] ofTime = Enumerable.Range(0, tasks.Length).Where(t => tasks[t].Time == time).ToArray();
            var groups = new[] { ("all", ofTime) }
                .Concat(ofTime.GroupBy(t => positions[tasks[t].Index].Board.EmptyCount).OrderByDescending(g => g.Key).Select(g => (g.Key.ToString(CultureInfo.InvariantCulture), g.ToArray())));
            foreach ((string empties, int[] ids) in groups)
            {
                for (int e = 0; e < engines.Length; e++)
                {
                    double[] losses = ids.Select(t => moves[e, t].Loss).ToArray();
                    results.Add(new QualityResult(engines[e], time, empties, ids.Length, losses.Average(), Margin(losses),
                        ids.Count(t => moves[e, t].Loss == 0) / (double)ids.Length,
                        ids.Count(t => moves[e, t].Solved) / (double)ids.Length,
                        ids.Average(t => moves[e, t].Ms)));
                }
            }

            for (int a = 0; a < engines.Length; a++)
            {
                for (int b = a + 1; b < engines.Length; b++)
                {
                    double[] diff = ofTime.Select(t => moves[a, t].Loss - moves[b, t].Loss).ToArray();
                    differences.Add(new QualityDifference(time, engines[a], engines[b], diff.Average(), Margin(diff)));
                }
            }
        }

        var run = new QualityRun(DateTime.Now.ToString("yyyy-MM-dd HH:mm", CultureInfo.InvariantCulture), Path.GetFileName(path), workers, results, differences);
        string markdown = Markdown(run, positions.Count);
        Console.WriteLine();
        Console.WriteLine(markdown);
        string folder = Path.Combine(Program.RepositoryFolder(), "Plans", "benchmark-results");
        Directory.CreateDirectory(folder);
        string basePath = Path.Combine(folder, name);
        File.WriteAllText(basePath + ".md", markdown);
        File.WriteAllText(basePath + ".json", JsonSerializer.Serialize(run, Json));
        Console.WriteLine($"Saved to {basePath}.md and .json");
        return 0;
    }

    private static double Margin(double[] values)
    {
        if (values.Length < 2)
        {
            return 0;
        }

        double mean = values.Average();
        return 1.96 * Math.Sqrt(values.Sum(v => (v - mean) * (v - mean)) / (values.Length - 1) / values.Length);
    }

    private static string Markdown(QualityRun run, int positions)
    {
        var text = new StringBuilder();
        text.AppendLine(CultureInfo.InvariantCulture, $"### mid-quality: {positions} positions from {run.Positions} ({run.Date}, {run.Workers} at a time)");
        text.AppendLine();
        text.AppendLine("Loss = discs lost against perfect play by the chosen move (95 % interval); best = the move was one of the best; solved = the search ended with an exact or win/loss/draw score.");
        text.AppendLine();
        text.AppendLine("| ms/move | Empty squares | Engine | Average loss | Best move | Solved | Time per move |");
        text.AppendLine("|---:|---|---|---:|---:|---:|---:|");
        foreach (QualityResult r in run.Results)
        {
            text.AppendLine(CultureInfo.InvariantCulture,
                $"| {r.TimeMs} | {r.Empties} | {r.Engine} | {r.AverageLoss:0.00} ± {r.LossMargin:0.00} | {r.BestMoveRate:P1} | {r.SolvedRate:P1} | {r.AverageMs:0} ms |");
        }

        if (run.Differences.Count > 0)
        {
            text.AppendLine();
            text.AppendLine("| ms/move | Engines | First's loss − second's (paired, discs) |");
            text.AppendLine("|---:|---|---:|");
            foreach (QualityDifference d in run.Differences)
            {
                text.AppendLine(CultureInfo.InvariantCulture, $"| {d.TimeMs} | {d.First} − {d.Second} | {d.Difference:+0.00;-0.00;0.00} ± {d.Margin:0.00} |");
            }
        }

        return text.ToString();
    }
}
