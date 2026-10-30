// SPDX-License-Identifier: GPL-3.0-only

using System.Diagnostics;
using System.Globalization;
using System.Numerics;
using System.Text;
using System.Text.Json;
using Stello.BookTool;
using Stello.Egaroucid;
using Stello.Engine;
using Stello.Engine.Evaluation;
using Board = Stello.Engine.Board;

namespace Stello.Benchmark;

/// <summary>How well one evaluation predicts the final score in a group of positions.</summary>
/// <param name="Scale">Stello: the fitted evaluation units per disc; Egaroucid: 1 (it evaluates in discs).</param>
/// <param name="Mae">Mean absolute error in discs, after scaling.</param>
/// <param name="Rmse">Root mean squared error in discs, after scaling.</param>
/// <param name="Correlation">Pearson correlation of the evaluation with the final score (does not depend on the scale).</param>
internal sealed record EvalError(double Scale, double Mae, double Rmse, double Correlation);

/// <param name="Empties">The group, as a range of empty squares such as "56-60".</param>
/// <param name="ZeroMae">The mean absolute final score: the error of always predicting a draw.</param>
internal sealed record EvalGroup(string Empties, int Positions, double ZeroMae, EvalError Egaroucid, EvalError Stello);

internal sealed record EvalCompareRun(
    string Date, string Data, int Positions, double EgaroucidPerSecond, double StelloPerSecond, double StelloScale,
    EvalError EgaroucidAll, EvalError StelloAll, EvalError StelloAllGlobalScale, IReadOnlyList<EvalGroup> Groups);

/// <summary>eval-compare: both evaluations against the final scores of Egaroucid's training games.</summary>
internal static class EvalCommands
{
    private const int GroupSize = 5;

    private static readonly JsonSerializerOptions Json = new() { WriteIndented = true };

    public static int EvalCompare(Options options)
    {
        options.Allow("data", "files", "per-empties", "name");
        string folder = options.Text("data") ?? Path.Combine(
            Program.RepositoryFolder(), "Traning data", "Egaroucid_Train_Data", "0001_egaroucid_7_5_1_lv17");
        if (!Directory.Exists(folder))
        {
            throw new ArgumentException($"The training data folder {folder} does not exist (give it with --data).");
        }

        int fileCount = options.Number("files", 1000, 1, 1000);
        int perEmpties = options.Number("per-empties", 5000, 10, 1_000_000);
        string name = options.Text("name") ?? "eval-compare";
        string[] files = Directory.GetFiles(folder, "*.txt").Order(StringComparer.Ordinal).Take(fileCount).ToArray();
        if (files.Length == 0)
        {
            throw new ArgumentException($"{folder} holds no .txt files.");
        }

        MatchCommands.LoadEgaroucid();
        List<(Board Board, int Score)> sample = Sample(files, perEmpties);
        Console.WriteLine($"eval-compare: {sample.Count:N0} positions from {files.Length} files, a random {perEmpties:N0} per number of empty squares.");

        var egaroucid = new int[sample.Count];
        var stello = new int[sample.Count];
        double egaroucidSeconds = Time(() =>
        {
            for (int i = 0; i < sample.Count; i++)
            {
                egaroucid[i] = Evaluate.MidEvaluate(EgaroucidPlayer.ToEgaroucid(sample[i].Board, Player.Black));
            }
        });
        double stelloSeconds = Time(() =>
        {
            for (int i = 0; i < sample.Count; i++)
            {
                Board board = sample[i].Board;
                int opponentMobility = BitOperations.PopCount(board.LegalMoves(Player.White));
                stello[i] = Evaluator.Evaluate(board, Player.Black, -2 * Evaluator.WinScore, 2 * Evaluator.WinScore, opponentMobility);
            }
        });

        int[] all = Enumerable.Range(0, sample.Count).ToArray();
        double globalScale = FitScale(all, stello, sample);
        var groups = all
            .GroupBy(i => (sample[i].Board.EmptyCount - 1) / GroupSize)
            .OrderByDescending(g => g.Key)
            .Select(g =>
            {
                int[] indices = g.ToArray();
                return new EvalGroup(
                    $"{g.Key * GroupSize + 1}-{g.Key * GroupSize + GroupSize}",
                    indices.Length,
                    indices.Average(i => Math.Abs(sample[i].Score)),
                    Error(indices, egaroucid, sample, 1),
                    Error(indices, stello, sample, FitScale(indices, stello, sample)));
            })
            .ToList();

        var run = new EvalCompareRun(
            DateTime.Now.ToString("yyyy-MM-dd HH:mm", CultureInfo.InvariantCulture),
            $"{Path.GetRelativePath(Program.RepositoryFolder(), folder)} ({files.Length} files)",
            sample.Count,
            sample.Count / egaroucidSeconds,
            sample.Count / stelloSeconds,
            globalScale,
            Error(all, egaroucid, sample, 1),
            Error(all, stello, sample, FitScale(all, stello, sample)),
            Error(all, stello, sample, globalScale),
            groups);

        string markdown = Markdown(run);
        Console.WriteLine();
        Console.WriteLine(markdown);
        string resultFolder = Path.Combine(Program.RepositoryFolder(), "Plans", "benchmark-results");
        Directory.CreateDirectory(resultFolder);
        string basePath = Path.Combine(resultFolder, name);
        File.WriteAllText(basePath + ".md", markdown);
        File.WriteAllText(basePath + ".json", JsonSerializer.Serialize(run, Json));
        Console.WriteLine($"Saved to {basePath}.md and .json");
        return 0;
    }

    /// <summary>
    /// Reads positions (<c>64 squares a1..h8, X = side to move</c>, then the final score for the side to move) and keeps
    /// a uniform random <paramref name="perEmpties"/> of each number of empty squares (reservoir sampling, fixed seed).
    /// </summary>
    private static List<(Board Board, int Score)> Sample(string[] files, int perEmpties)
    {
        var random = new Random(1);
        var seen = new long[61];
        var reservoirs = Enumerable.Range(0, 61).Select(_ => new List<string>()).ToArray();
        foreach (string file in files)
        {
            foreach (string line in File.ReadLines(file))
            {
                if (line.Length < 66 || line[64] != ' ')
                {
                    continue;
                }

                int empties = line.AsSpan(0, 64).Count('-');
                if (empties is < 1 or > 60)
                {
                    continue;
                }

                List<string> reservoir = reservoirs[empties];
                long index = seen[empties]++;
                if (reservoir.Count < perEmpties)
                {
                    reservoir.Add(line);
                }
                else if (random.NextInt64(index + 1) is var slot && slot < perEmpties)
                {
                    reservoir[(int)slot] = line;
                }
            }
        }

        var sample = new List<(Board, int)>();
        foreach (string line in reservoirs.SelectMany(r => r))
        {
            ulong own = 0;
            ulong opponent = 0;
            for (int square = 0; square < 64; square++)
            {
                switch (line[square])
                {
                    case 'X':
                        own |= 1UL << square;
                        break;
                    case 'O':
                        opponent |= 1UL << square;
                        break;
                }
            }

            if (int.TryParse(line.AsSpan(65), NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out int score))
            {
                sample.Add((new Board(own, opponent), score));
            }
        }

        return sample;
    }

    /// <summary>The units per disc that minimise the squared error of evaluation / scale against the score.</summary>
    private static double FitScale(int[] indices, int[] values, List<(Board Board, int Score)> sample)
    {
        double ee = 0;
        double ey = 0;
        foreach (int i in indices)
        {
            ee += (double)values[i] * values[i];
            ey += (double)values[i] * sample[i].Score;
        }

        return ey > 0 ? ee / ey : double.NaN;
    }

    private static EvalError Error(int[] indices, int[] values, List<(Board Board, int Score)> sample, double scale)
    {
        double absolute = 0;
        double squared = 0;
        double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
        foreach (int i in indices)
        {
            double x = values[i];
            double y = sample[i].Score;
            double error = x / scale - y;
            absolute += Math.Abs(error);
            squared += error * error;
            sx += x;
            sy += y;
            sxx += x * x;
            syy += y * y;
            sxy += x * y;
        }

        int n = indices.Length;
        double covariance = sxy - sx * sy / n;
        double correlation = covariance / Math.Sqrt((sxx - sx * sx / n) * (syy - sy * sy / n));
        return new EvalError(scale, absolute / n, Math.Sqrt(squared / n), correlation);
    }

    private static double Time(Action work)
    {
        work(); // warm-up and JIT
        var clock = Stopwatch.StartNew();
        work();
        return clock.Elapsed.TotalSeconds;
    }

    private static string Markdown(EvalCompareRun run)
    {
        var text = new StringBuilder();
        text.AppendLine(CultureInfo.InvariantCulture, $"### eval-compare: {run.Positions:N0} positions ({run.Date})");
        text.AppendLine();
        text.AppendLine(CultureInfo.InvariantCulture, $"Data: {run.Data}. The target is the final score of the game for the side to move, in discs.");
        text.AppendLine(CultureInfo.InvariantCulture,
            $"Stello's evaluation is divided by a fitted scale per group (least squares); over all positions the scale is {run.StelloScale:0.0} units per disc.");
        text.AppendLine();
        text.AppendLine("| Empty squares | Positions | Draw guess MAE | Egaroucid MAE | Egaroucid RMSE | Egaroucid r | Stello units/disc | Stello MAE | Stello RMSE | Stello r |");
        text.AppendLine("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|");
        foreach (EvalGroup g in run.Groups)
        {
            text.AppendLine(CultureInfo.InvariantCulture,
                $"| {g.Empties} | {g.Positions:N0} | {g.ZeroMae:0.00} | {g.Egaroucid.Mae:0.00} | {g.Egaroucid.Rmse:0.00} | {g.Egaroucid.Correlation:0.000} | " +
                $"{g.Stello.Scale:0.0} | {g.Stello.Mae:0.00} | {g.Stello.Rmse:0.00} | {g.Stello.Correlation:0.000} |");
        }

        text.AppendLine(CultureInfo.InvariantCulture,
            $"| **All** | {run.Positions:N0} | | **{run.EgaroucidAll.Mae:0.00}** | **{run.EgaroucidAll.Rmse:0.00}** | **{run.EgaroucidAll.Correlation:0.000}** | " +
            $"{run.StelloScale:0.0} | **{run.StelloAllGlobalScale.Mae:0.00}** | **{run.StelloAllGlobalScale.Rmse:0.00}** | **{run.StelloAllGlobalScale.Correlation:0.000}** |");
        text.AppendLine();
        text.AppendLine(CultureInfo.InvariantCulture,
            $"Evaluations per second (from scratch, 1 thread): Egaroucid {run.EgaroucidPerSecond / 1e6:0.00}M, Stello {run.StelloPerSecond / 1e6:0.00}M.");
        return text.ToString();
    }
}
