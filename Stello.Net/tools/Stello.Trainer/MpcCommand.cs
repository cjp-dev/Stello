using System.Diagnostics;
using System.Globalization;
using System.Numerics;
using System.Text;
using Stello.BookTool;
using Stello.Engine;
using Stello.Engine.Evaluation;
using Stello.Engine.Search;

namespace Stello.Trainer;

/// <summary>
/// mpc: the data for Multi-ProbCut. Test positions are searched without selectivity to every depth up to
/// <c>--max-depth</c>; the spread of deep minus shallow values gives the error model.
/// </summary>
internal static class MpcCommand
{
    private const int BucketDiscs = 4;
    private const int GameEnd = 32000;

    public static int Run(Options options)
    {
        options.Allow("data", "from", "to", "per-discs", "max-depth", "workers", "out", "fit");
        string data = options.Text("data") ?? TrainingData.DefaultFolder();
        int from = options.Number("from", 14, 8, 56);
        int to = options.Number("to", 52, from, 56);
        int perDiscs = options.Number("per-discs", 100, 1, 100_000);
        int maxDepth = options.Number("max-depth", 12, 1, 20);
        int workers = options.Number("workers", Environment.ProcessorCount, 1, 256);
        string output = options.Text("out") ?? Path.Combine(TrainerProgram.RepositoryFolder(), "Traning data", "mpc-values.csv");

        // --fit <csv>: only fit an existing file.
        if (options.Text("fit") is { } existing)
        {
            Fit(ReadValues(existing));
            return 0;
        }

        var positions = new List<Sample>();
        for (int discs = from; discs <= to; discs++)
        {
            Sample[] samples = TrainingData.Load(data, Split.Test, discs, 50_000)
                .Where(s => Bitboards.LegalMoves(s.Own, s.Opponent) != 0)
                .ToArray();
            int step = Math.Max(1, samples.Length / perDiscs);
            positions.AddRange(samples.Where((_, i) => i % step == 0).Take(perDiscs));
        }

        Console.WriteLine($"{positions.Count:N0} positions with {from}-{to} discs, depths 0-{maxDepth}, {workers} workers.");
        var values = new int[positions.Count][];
        var clock = Stopwatch.StartNew();
        int done = 0;
        var options2 = SearchOptions.Default with { Selective = false };
        Parallel.For(0, positions.Count, new ParallelOptions { MaxDegreeOfParallelism = workers },
            () => new SearchEngine(18, options2),
            (i, _, engine) =>
            {
                Sample sample = positions[i];
                int empties = 64 - sample.Discs;
                int depth = Math.Min(maxDepth, empties - 8);
                var row = new int[depth + 1];
                row[0] = EvalWeights.Default.Evaluate(sample.Own, sample.Opponent) * 100 / EvalWeights.Default.UnitsPerDisc;
                engine.ClearHash();
                engine.IterationDone = (d, score) =>
                {
                    if (d <= depth)
                    {
                        row[d] = score;
                    }
                };
                engine.Search(new Board(sample.Own, sample.Opponent), Player.Black, SearchLimits.FixedDepth(depth));
                values[i] = row;
                int finished = Interlocked.Increment(ref done);
                if (finished % 200 == 0)
                {
                    Console.WriteLine($"  {finished:N0}/{positions.Count:N0}, {clock.Elapsed:m\\:ss}");
                }

                return engine;
            },
            _ => { });

        var text = new StringBuilder("discs,values (1/100 disc) for depth 0, 1, ...\n");
        for (int i = 0; i < positions.Count; i++)
        {
            text.Append(positions[i].Discs).Append(',').AppendJoin(',', values[i]).Append('\n');
        }

        File.WriteAllText(output, text.ToString());
        Console.WriteLine($"Wrote {output} in {clock.Elapsed:m\\:ss}.");
        Fit(ReadValues(output));
        return 0;
    }

    private static List<(int Discs, int[] Values)> ReadValues(string path) =>
        File.ReadLines(path).Skip(1)
            .Select(line => line.Split(','))
            .Select(parts => (int.Parse(parts[0], CultureInfo.InvariantCulture), parts.Skip(1).Select(p => int.Parse(p, CultureInfo.InvariantCulture)).ToArray()))
            .ToList();

    // Egaroucid's shallow depth for a deep one: about half, with the same parity (multi_probcut.hpp).
    public static int ShallowDepth(int depth) => ((depth / 2) & ~1) + (depth & 1);

    private static void Fit(List<(int Discs, int[] Values)> rows)
    {
        int maxDepth = rows.Max(r => r.Values.Length - 1);
        int buckets = 64 / BucketDiscs + 1;

        // sums[bucket, shallow, deep] of (deep - shallow) and its square, in discs.
        var sum = new double[buckets, maxDepth + 1, maxDepth + 1];
        var square = new double[buckets, maxDepth + 1, maxDepth + 1];
        var count = new int[buckets, maxDepth + 1, maxDepth + 1];
        foreach ((int discs, int[] values) in rows)
        {
            int bucket = discs / BucketDiscs;
            for (int deep = 1; deep < values.Length; deep++)
            {
                for (int shallow = 0; shallow < deep; shallow++)
                {
                    // A forced game end found by the search is not an evaluation error; Multi-ProbCut skips those.
                    if (Math.Abs(values[deep]) >= GameEnd || Math.Abs(values[shallow]) >= GameEnd)
                    {
                        continue;
                    }

                    double difference = (values[deep] - values[shallow]) / 100.0;
                    sum[bucket, shallow, deep] += difference;
                    square[bucket, shallow, deep] += difference * difference;
                    count[bucket, shallow, deep]++;
                }
            }
        }

        double Sigma(int b, int s, int d) => Math.Sqrt(square[b, s, d] / count[b, s, d]);

        Console.WriteLine();
        Console.WriteLine("σ of deep − shallow (discs) for Egaroucid's shallow depth, by discs (rows) and deep depth (columns):");
        var header = new StringBuilder("| Discs |");
        for (int d = 2; d <= maxDepth; d++)
        {
            header.Append(CultureInfo.InvariantCulture, $" {ShallowDepth(d)}→{d} |");
        }

        Console.WriteLine(header);
        Console.WriteLine("|---|" + string.Concat(Enumerable.Repeat("---:|", maxDepth - 1)));
        for (int b = 0; b < buckets; b++)
        {
            if (count[b, 0, 1] == 0)
            {
                continue;
            }

            var line = new StringBuilder(string.Create(CultureInfo.InvariantCulture, $"| {b * BucketDiscs}-{b * BucketDiscs + BucketDiscs - 1} |"));
            for (int d = 2; d <= maxDepth; d++)
            {
                int s = ShallowDepth(d);
                line.Append(count[b, s, d] >= 20 ? string.Create(CultureInfo.InvariantCulture, $" {Sigma(b, s, d):0.00} |") : " |");
            }

            Console.WriteLine(line);
        }

        // Least squares: σ ≈ c0 + c1 x + c2 x² + c3 s + c4 d with x = discs / 64, over every cell with enough data,
        // weighted by its count.
        var features = new List<double[]>();
        var targets = new List<double>();
        var weights = new List<double>();
        for (int b = 0; b < buckets; b++)
        {
            for (int d = 2; d <= maxDepth; d++)
            {
                int s = ShallowDepth(d);
                if (count[b, s, d] >= 20)
                {
                    double x = (b * BucketDiscs + BucketDiscs / 2.0) / 64.0;
                    features.Add([1, x, x * x, s, d]);
                    targets.Add(Sigma(b, s, d));
                    weights.Add(count[b, s, d]);
                }
            }
        }

        if (features.Count < 5)
        {
            Console.WriteLine("Too few positions per cell to fit (at least 20 per group of 4 disc counts are needed).");
            return;
        }

        double[] c = LeastSquares(features, targets, weights);
        double worst = 0;
        double total = 0;
        for (int i = 0; i < features.Count; i++)
        {
            double predicted = Enumerable.Range(0, c.Length).Sum(k => c[k] * features[i][k]);
            double relative = Math.Abs(predicted - targets[i]) / targets[i];
            worst = Math.Max(worst, relative);
            total += relative * weights[i];
        }

        Console.WriteLine();
        Console.WriteLine(string.Create(CultureInfo.InvariantCulture,
            $"Fit: σ = {c[0]:0.0000} + {c[1]:0.0000} x + {c[2]:0.0000} x² + {c[3]:0.0000} s + {c[4]:0.0000} d  (x = discs / 64, s and d in plies)"));
        Console.WriteLine(string.Create(CultureInfo.InvariantCulture,
            $"Relative error: {total / weights.Sum():P1} on average, {worst:P1} at worst, over {features.Count} cells."));
        Console.WriteLine(string.Create(CultureInfo.InvariantCulture,
            $"Mean of deep − shallow over all cells: {Enumerable.Range(0, buckets).Sum(b => Enumerable.Range(2, maxDepth - 1).Sum(d => sum[b, ShallowDepth(d), d])) / Enumerable.Range(0, buckets).Sum(b => Enumerable.Range(2, maxDepth - 1).Sum(d => count[b, ShallowDepth(d), d])):0.00} discs"));
    }

    private static double[] LeastSquares(List<double[]> features, List<double> targets, List<double> weights)
    {
        int n = features[0].Length;
        var a = new double[n, n + 1];
        for (int i = 0; i < features.Count; i++)
        {
            for (int j = 0; j < n; j++)
            {
                for (int k = 0; k < n; k++)
                {
                    a[j, k] += weights[i] * features[i][j] * features[i][k];
                }

                a[j, n] += weights[i] * features[i][j] * targets[i];
            }
        }

        // Gaussian elimination with partial pivoting.
        for (int col = 0; col < n; col++)
        {
            int pivot = col;
            for (int row = col + 1; row < n; row++)
            {
                if (Math.Abs(a[row, col]) > Math.Abs(a[pivot, col]))
                {
                    pivot = row;
                }
            }

            for (int k = 0; k <= n; k++)
            {
                (a[col, k], a[pivot, k]) = (a[pivot, k], a[col, k]);
            }

            for (int row = 0; row < n; row++)
            {
                if (row != col)
                {
                    double factor = a[row, col] / a[col, col];
                    for (int k = col; k <= n; k++)
                    {
                        a[row, k] -= factor * a[col, k];
                    }
                }
            }
        }

        return Enumerable.Range(0, n).Select(i => a[i, n] / a[i, i]).ToArray();
    }
}
