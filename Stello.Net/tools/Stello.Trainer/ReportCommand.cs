using System.Buffers.Binary;
using System.Diagnostics;
using System.Globalization;
using System.Numerics;
using System.Text;
using System.Text.Json;
using Stello.BookTool;
using Stello.Egaroucid;
using Stello.Engine;
using Stello.Engine.Evaluation;
using V1Evaluator = Stello.Engine.V1.Evaluation.Evaluator;

namespace Stello.Trainer;

/// <summary>One evaluation's error against the final scores of a group of test positions.</summary>
/// <param name="Scale">Units per disc: the model's own, Stello 1's fitted per group (least squares), Egaroucid's 1.</param>
internal sealed record ModelError(double Scale, double Mae, double Rmse);

internal sealed record ReportGroup(string Empties, int Positions, double DrawMae, ModelError Model, ModelError StelloOne, ModelError? Egaroucid);

internal sealed record ReportRun(string Date, string Model, long ModelBytes, int Positions, double ModelPerSecond, ModelError ModelAll,
    ModelError StelloOneAll, ModelError? EgaroucidAll, IReadOnlyList<ReportGroup> Groups);

/// <summary>report: a model's error on the test split, next to Stello 1 and the Egaroucid port.</summary>
internal static class ReportCommand
{
    private const int GroupSize = 5;

    public static int Run(Options options)
    {
        options.Allow("model", "data", "max-per-discs", "name", "egaroucid");
        string modelPath = options.Text("model") ?? throw new ArgumentException("report needs --model <file>.");
        string data = options.Text("data") ?? TrainingData.DefaultFolder();
        int maxPerDiscs = options.Number("max-per-discs", 20_000, 100, 10_000_000);
        string name = options.Text("name") ?? "eval-" + Path.GetFileNameWithoutExtension(modelPath);
        bool useEgaroucid = (options.Text("egaroucid") ?? "on") == "on";

        EvalWeights model = EvalWeights.Load(modelPath);
        Sample[] samples = Enumerable.Range(TrainingData.MinDiscs, TrainingData.MaxDiscs - TrainingData.MinDiscs + 1)
            .SelectMany(d => TrainingData.Load(data, Split.Test, d, maxPerDiscs)).ToArray();
        if (samples.Length == 0)
        {
            throw new ArgumentException($"{data} holds no test positions (make them with convert).");
        }

        if (useEgaroucid)
        {
            EgaroucidResources.Load();
        }

        var modelValues = new double[samples.Length];
        var stelloOne = new double[samples.Length];
        double[]? egaroucid = useEgaroucid ? new double[samples.Length] : null;
        var clock = Stopwatch.StartNew();
        for (int i = 0; i < samples.Length; i++)
        {
            modelValues[i] = model.Evaluate(samples[i].Own, samples[i].Opponent);
        }

        double modelPerSecond = samples.Length / clock.Elapsed.TotalSeconds;
        Parallel.For(0, samples.Length, i =>
        {
            var board = new Engine.Board(samples[i].Own, samples[i].Opponent);
            int opponentMobility = BitOperations.PopCount(board.LegalMoves(Player.White));
            stelloOne[i] = V1Evaluator.Evaluate(board, Player.Black, -2 * V1Evaluator.WinScore, 2 * V1Evaluator.WinScore, opponentMobility);
            if (egaroucid is not null)
            {
                egaroucid[i] = Evaluate.MidEvaluate(new Egaroucid.Board(ReverseBits(samples[i].Own), ReverseBits(samples[i].Opponent)));
            }
        });

        int[] all = Enumerable.Range(0, samples.Length).ToArray();
        var groups = all.GroupBy(i => (64 - samples[i].Discs - 1) / GroupSize)
            .Where(g => g.Key >= 0)
            .OrderByDescending(g => g.Key)
            .Select(g =>
            {
                int[] ids = g.ToArray();
                return new ReportGroup($"{g.Key * GroupSize + 1}-{g.Key * GroupSize + GroupSize}", ids.Length,
                    ids.Average(i => Math.Abs((double)samples[i].Score)),
                    Error(ids, modelValues, samples, model.UnitsPerDisc),
                    Error(ids, stelloOne, samples, FitScale(ids, stelloOne, samples)),
                    egaroucid is null ? null : Error(ids, egaroucid, samples, 1));
            })
            .ToList();

        var run = new ReportRun(DateTime.Now.ToString("yyyy-MM-dd HH:mm", CultureInfo.InvariantCulture), Path.GetFileName(modelPath),
            new FileInfo(modelPath).Length, samples.Length, modelPerSecond,
            Error(all, modelValues, samples, model.UnitsPerDisc),
            Error(all, stelloOne, samples, FitScale(all, stelloOne, samples)),
            egaroucid is null ? null : Error(all, egaroucid, samples, 1),
            groups);

        string markdown = Markdown(run, model);
        Console.WriteLine(markdown);
        string folder = Path.Combine(TrainerProgram.RepositoryFolder(), "Plans", "benchmark-results");
        Directory.CreateDirectory(folder);
        File.WriteAllText(Path.Combine(folder, name + ".md"), markdown);
        File.WriteAllText(Path.Combine(folder, name + ".json"), JsonSerializer.Serialize(run, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"Saved to {Path.Combine(folder, name)}.md and .json");
        return 0;
    }

    private static double FitScale(int[] ids, double[] values, Sample[] samples)
    {
        double ee = 0;
        double ey = 0;
        foreach (int i in ids)
        {
            ee += values[i] * values[i];
            ey += values[i] * samples[i].Score;
        }

        return ey > 0 ? ee / ey : double.NaN;
    }

    private static ModelError Error(int[] ids, double[] values, Sample[] samples, double scale)
    {
        double absolute = 0;
        double squared = 0;
        foreach (int i in ids)
        {
            double error = values[i] / scale - samples[i].Score;
            absolute += Math.Abs(error);
            squared += error * error;
        }

        return new ModelError(scale, absolute / ids.Length, Math.Sqrt(squared / ids.Length));
    }

    private static string Markdown(ReportRun run, EvalWeights model)
    {
        var text = new StringBuilder();
        text.AppendLine(CultureInfo.InvariantCulture, $"### Evaluation report: {run.Model} ({run.Date})");
        text.AppendLine();
        text.AppendLine(CultureInfo.InvariantCulture,
            $"Model: pattern set {model.Set.Name}, {model.Stages} stages, {run.ModelBytes:N0} bytes. Test positions: {run.Positions:N0} (the test split of the converted training data, not used in training). Target: the final disc difference for the side to move.");
        text.AppendLine("Stello 1's evaluation is divided by a fitted scale per group; Egaroucid's is in discs (and was trained on data that includes these games).");
        text.AppendLine();
        text.AppendLine("| Empty squares | Positions | Draw guess MAE | Model MAE | Model RMSE | Stello 1 MAE | Stello 1 RMSE | Egaroucid MAE | Egaroucid RMSE |");
        text.AppendLine("|---|---:|---:|---:|---:|---:|---:|---:|---:|");
        foreach (ReportGroup g in run.Groups)
        {
            text.AppendLine(string.Create(CultureInfo.InvariantCulture,
                $"| {g.Empties} | {g.Positions:N0} | {g.DrawMae:0.00} | {g.Model.Mae:0.00} | {g.Model.Rmse:0.00} | {g.StelloOne.Mae:0.00} | {g.StelloOne.Rmse:0.00} | {Number(g.Egaroucid?.Mae)} | {Number(g.Egaroucid?.Rmse)} |"));
        }

        text.AppendLine(string.Create(CultureInfo.InvariantCulture,
            $"| **All** | {run.Positions:N0} | | **{run.ModelAll.Mae:0.00}** | **{run.ModelAll.Rmse:0.00}** | **{run.StelloOneAll.Mae:0.00}** | **{run.StelloOneAll.Rmse:0.00}** | **{Number(run.EgaroucidAll?.Mae)}** | **{Number(run.EgaroucidAll?.Rmse)}** |"));
        text.AppendLine();
        text.AppendLine(CultureInfo.InvariantCulture, $"Model evaluations per second (from scratch, 1 thread): {run.ModelPerSecond / 1e6:0.00}M.");
        return text.ToString();
    }

    private static string Number(double? value) => value is null ? "—" : value.Value.ToString("0.00", CultureInfo.InvariantCulture);

    /// <summary>Stello's bit order (a1 = bit 0) as Egaroucid's (a1 = bit 63).</summary>
    private static ulong ReverseBits(ulong x)
    {
        x = ((x >> 1) & 0x5555555555555555UL) | ((x & 0x5555555555555555UL) << 1);
        x = ((x >> 2) & 0x3333333333333333UL) | ((x & 0x3333333333333333UL) << 2);
        x = ((x >> 4) & 0x0F0F0F0F0F0F0F0FUL) | ((x & 0x0F0F0F0F0F0F0F0FUL) << 4);
        return BinaryPrimitives.ReverseEndianness(x);
    }
}
