using System.Diagnostics;
using System.Globalization;
using System.Numerics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Stello.BookTool;
using Stello.Engine;
using Stello.Engine.Evaluation;

namespace Stello.Trainer;

/// <summary>
/// train: fits the weights of a pattern set to the final scores, stage by stage, with mean squared error.
/// <list type="bullet">
/// <item>The weights a symmetry makes equal are one weight (<see cref="FoldedIndex"/>).</item>
/// <item>A stage is trained on its own disc counts and <c>--smooth</c> disc counts on each side, so neighbouring
/// stages agree more and the sparse early stages get more data. It is validated on its own disc counts only.</item>
/// <item>Full-batch Adam on the mean residual of each weight, with a small pull towards zero for rarely seen weights.
/// When the validation error has not improved for <c>--patience</c> loops, the rate is halved; the best weights on
/// validation are kept.</item>
/// </list>
/// </summary>
internal static class TrainCommand
{
    private const double Beta1 = 0.9;
    private const double Beta2 = 0.999;
    private const double Epsilon = 1e-8;

    public static int Run(Options options)
    {
        options.Allow("out", "set", "stages", "data", "max-per-discs", "min-discs", "max-discs", "smooth", "loops", "rate", "patience", "workers",
            "only-stage", "units", "prior", "min-appear", "mobility");
        string output = options.Text("out") ?? throw new ArgumentException("train needs --out <file>.");
        PatternSet set = PatternSet.ByName(options.Text("set") ?? "classic11");
        int stages = options.Number("stages", 15, 1, 61);
        string data = options.Text("data") ?? TrainingData.DefaultFolder();
        int maxPerDiscs = options.Number("max-per-discs", 2_000_000, 1000, 100_000_000);
        int minDiscs = options.Number("min-discs", TrainingData.MinDiscs, TrainingData.MinDiscs, TrainingData.MaxDiscs);
        int maxDiscs = options.Number("max-discs", TrainingData.MaxDiscs, minDiscs, TrainingData.MaxDiscs);
        int smooth = options.Number("smooth", 2, 0, 20);
        int loops = options.Number("loops", 1000, 1, 100_000);
        double rate = Fraction(options, "rate", 0.05);
        double prior = Fraction(options, "prior", 1.0);
        int patience = options.Number("patience", 20, 1, 10_000);
        int workers = options.Number("workers", Environment.ProcessorCount, 1, 256);
        int units = options.Number("units", 32, 1, 1024);
        int onlyStage = options.Number("only-stage", -1, -1, stages - 1);
        int minAppear = options.Number("min-appear", 0, 0, 1_000_000);
        string mobArg = options.Text("mobility") ?? "off";
        bool mobility = mobArg switch
        {
            "on" => true,
            "off" => false,
            _ => throw new ArgumentException("--mobility must be on or off."),
        };
        if (!Directory.Exists(data))
        {
            throw new ArgumentException($"The training data folder {data} does not exist (make it with convert).");
        }

        byte[] stageOf = EvalWeights.EvenStages(stages);
        var folded = new FoldedIndex(set);
        int featureCount = set.FeatureCount + (mobility ? 2 : 0);
        int weightCount = folded.Count + (mobility ? EvalWeights.MobilityWeightsPerStage : 0);
        var trained = new double[]?[stages];
        var summary = new List<string>();
        Console.WriteLine($"train: {set.Name} ({featureCount} features, {weightCount:N0} weights per stage{(mobility ? ", +mobility" : "")}), {stages} stages, " +
            $"up to {maxPerDiscs:N0} positions per disc count, smooth ±{smooth}, rate {rate}, {workers} workers.");

        var clock = Stopwatch.StartNew();
        for (int stage = 0; stage < stages; stage++)
        {
            if (onlyStage >= 0 && stage != onlyStage)
            {
                continue;
            }

            trained[stage] = TrainStage(stage, stageOf, minDiscs, maxDiscs, smooth, data, maxPerDiscs, folded, mobility, loops, rate, prior, patience, workers, minAppear, clock, summary);
        }

        // Stages without data take the weights of the nearest stage that has them.
        int[] done = Enumerable.Range(0, stages).Where(s => trained[s] is not null).ToArray();
        if (done.Length == 0)
        {
            throw new ArgumentException("No stage had enough training positions.");
        }

        var weightsByStage = new short[stages][];
        short[][]? mobilityByStage = mobility ? new short[stages][] : null;
        for (int stage = 0; stage < stages; stage++)
        {
            int source = done.MinBy(s => Math.Abs(s - stage));
            weightsByStage[stage] = folded.Expand(trained[source]!, units);
            if (mobilityByStage is not null)
            {
                mobilityByStage[stage] = ExpandMobility(trained[source]!, folded.Count, units);
            }
        }

        var model = new EvalWeights(set, stageOf, units, weightsByStage, mobilityByStage);
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        using (FileStream file = File.Create(output))
        {
            model.Write(file);
        }

        long size = new FileInfo(output).Length;
        Console.WriteLine();
        Console.WriteLine("| Stage | Discs | Training positions | Loops | Training MAE | Validation MAE | Validation RMSE |");
        Console.WriteLine("|---:|---|---:|---:|---:|---:|---:|");
        summary.ForEach(Console.WriteLine);
        Console.WriteLine();
        Console.WriteLine($"Wrote {output}: {size:N0} bytes (gzip), {stages} stages × {folded.Count:N0} weights, {clock.Elapsed:h\\:mm\\:ss}.");
        return 0;
    }

    private static double[]? TrainStage(
        int stage,
        byte[] stageOf,
        int minDiscs,
        int maxDiscs,
        int smooth,
        string data,
        int maxPerDiscs,
        FoldedIndex folded,
        bool mobility,
        int loops,
        double rate,
        double prior,
        int patience,
        int workers,
        int minAppear,
        Stopwatch clock,
        List<string> summary)
    {
        int[] own = Enumerable.Range(minDiscs, maxDiscs - minDiscs + 1).Where(d => stageOf[d] == stage).ToArray();
        if (own.Length == 0)
        {
            summary.Add($"| {stage} | — | 0 | — | — | — | skipped |");
            return null;
        }

        int[] trainDiscs = Enumerable.Range(own[0] - smooth, own[^1] - own[0] + 1 + 2 * smooth)
            .Where(d => d is >= TrainingData.MinDiscs and <= TrainingData.MaxDiscs).ToArray();
        Encoded train = Encode(folded, trainDiscs.SelectMany(d => TrainingData.Load(data, Split.Train, d, maxPerDiscs)).ToArray(), mobility, workers);
        Encoded validation = Encode(folded, own.SelectMany(d => TrainingData.Load(data, Split.Validation, d, maxPerDiscs)).ToArray(), mobility, workers);
        if (train.Count < 1000 || validation.Count < 100)
        {
            Console.WriteLine($"stage {stage} (discs {own[0]}–{own[^1]}): too few positions ({train.Count:N0}), filled from a neighbour.");
            summary.Add($"| {stage} | {own[0]}–{own[^1]} | {train.Count:N0} | — | — | — | filled from a neighbour |");
            return null;
        }

        int weightCount = folded.Count + (mobility ? EvalWeights.MobilityWeightsPerStage : 0);
        (double[] weights, Fit fit) = Fit1(weightCount, train, validation, loops, rate, prior, patience, workers, minAppear);
        Console.WriteLine($"stage {stage} (discs {own[0]}–{own[^1]}): {train.Count:N0} train, {validation.Count:N0} validation, " +
            $"{fit.Loops} loops, train MAE {fit.TrainMae:0.000}, validation MAE {fit.ValidationMae:0.000} (RMSE {Math.Sqrt(fit.ValidationMse):0.000}), {clock.Elapsed:h\\:mm\\:ss}");
        summary.Add($"| {stage} | {own[0]}–{own[^1]} | {train.Count:N0} | {fit.Loops} | {fit.TrainMae:0.000} | {fit.ValidationMae:0.000} | {Math.Sqrt(fit.ValidationMse):0.000} |");
        return weights;
    }

    private static short[] ExpandMobility(double[] weights, int offset, int units)
    {
        var table = new short[EvalWeights.MobilityWeightsPerStage];
        for (int i = 0; i < table.Length; i++)
        {
            table[i] = (short)Math.Clamp(Math.Round(weights[offset + i] * units, MidpointRounding.AwayFromZero), short.MinValue, short.MaxValue);
        }

        return table;
    }

    private static double Fraction(Options options, string name, double defaultValue)
    {
        string? text = options.Text(name);
        return text is null ? defaultValue
            : double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double value) && value > 0 ? value
            : throw new ArgumentException($"--{name} must be a positive number.");
    }

    /// <summary>
    /// Positions as their folded weight numbers and scores. A feature's number is stored as 16 bits relative to the
    /// first folded number of its shape (<see cref="FoldedIndex.FeatureBase"/>; a shape has at most 3^10 = 59,049
    /// raw indices), which halves the memory against 32-bit numbers. The rows are kept in blocks, so that more than
    /// 2^31 numbers (34 M positions of 62 features) fit.
    /// </summary>
    private sealed class Encoded
    {
        private const int BlockRows = 1 << 22;
        private readonly ushort[][] _blocks;

        public Encoded(int count, int[] featureBase, float[] scores)
        {
            Count = count;
            FeatureBase = featureBase;
            Scores = scores;
            int features = featureBase.Length;
            _blocks = new ushort[(count + BlockRows - 1) / BlockRows][];
            for (int b = 0; b < _blocks.Length; b++)
            {
                _blocks[b] = new ushort[(long)Math.Min(BlockRows, count - (long)b * BlockRows) * features];
            }
        }

        public int Count { get; }

        public int Features => FeatureBase.Length;

        /// <summary>The first folded number of each feature's shape.</summary>
        public int[] FeatureBase { get; }

        public float[] Scores { get; }

        public int BlockSize => BlockRows;

        public ushort[][] Blocks => _blocks;

        /// <summary>The relative numbers of position <paramref name="i"/>, one per feature.</summary>
        public Span<ushort> Row(int i) => _blocks[i / BlockRows].AsSpan(i % BlockRows * Features, Features);
    }

    private static Encoded Encode(FoldedIndex folded, Sample[] samples, bool mobility, int workers)
    {
        int patternFeatures = folded.Set.FeatureCount;
        int[] featureBase = mobility
            ? [.. folded.FeatureBase, folded.Count, folded.Count + EvalWeights.MobilitySlotsPerSide]
            : folded.FeatureBase;
        var encoded = new Encoded(samples.Length, featureBase, new float[samples.Length]);
        Parallel.For(0, workers, new ParallelOptions { MaxDegreeOfParallelism = workers }, worker =>
        {
            Span<int> raw = stackalloc int[patternFeatures];
            for (int i = worker; i < samples.Length; i += workers)
            {
                ulong own = samples[i].Own;
                ulong opp = samples[i].Opponent;
                folded.Set.Indices(own, opp, raw);
                Span<ushort> row = encoded.Row(i);
                for (int f = 0; f < patternFeatures; f++)
                {
                    row[f] = (ushort)(folded.Of(raw[f]) - featureBase[f]);
                }

                if (mobility)
                {
                    row[patternFeatures] = (ushort)Math.Min(EvalWeights.MobilitySlotsPerSide - 1, BitOperations.PopCount(Bitboards.LegalMoves(own, opp)));
                    row[patternFeatures + 1] = (ushort)Math.Min(EvalWeights.MobilitySlotsPerSide - 1, BitOperations.PopCount(Bitboards.LegalMoves(opp, own)));
                }

                encoded.Scores[i] = samples[i].Score;
            }
        });

        return encoded;
    }

    private sealed record Fit(int Loops, double TrainMae, double ValidationMae, double ValidationMse);

    private static (double[] Weights, Fit Fit) Fit1(int count, Encoded train, Encoded validation, int loops, double rate,
        double prior, int patience, int workers, int minAppear)
    {
        var weights = new double[count];
        var best = new double[count];
        var m = new double[count];
        var v = new double[count];
        var appear = new int[count];
        int[] featureBase = train.FeatureBase;
        for (int i = 0; i < train.Count; i++)
        {
            ReadOnlySpan<ushort> row = train.Row(i);
            for (int f = 0; f < row.Length; f++)
            {
                appear[featureBase[f] + row[f]]++;
            }
        }

        double[][] partial = Enumerable.Range(0, workers).Select(_ => new double[count]).ToArray();
        var errors = new double[workers * 2];
        double bestMse = double.MaxValue;
        double bestTrainMae = 0;
        double bestMae = 0;
        int sinceBest = 0;
        int loop;
        double step = rate;
        for (loop = 1; loop <= loops && step >= rate / 64; loop++)
        {
            Array.Clear(errors);
            Parallel.For(0, workers, new ParallelOptions { MaxDegreeOfParallelism = workers }, worker =>
            {
                double[] sum = partial[worker];
                Array.Clear(sum);
                double squared = 0;
                double absolute = 0;
                int first = (int)((long)train.Count * worker / workers);
                int last = (int)((long)train.Count * (worker + 1) / workers);
                int features = train.Features;
                Span<int> indices = stackalloc int[features];
                ref int baseRef = ref MemoryMarshal.GetArrayDataReference(featureBase);
                ref double wRef = ref MemoryMarshal.GetArrayDataReference(weights);
                ref double sRef = ref MemoryMarshal.GetArrayDataReference(sum);
                ref float scoreRef = ref MemoryMarshal.GetArrayDataReference(train.Scores);
                ref int idxRef = ref MemoryMarshal.GetReference(indices);

                int blockSize = train.BlockSize;
                for (int i = first; i < last;)
                {
                    int b = i / blockSize;
                    int rowInBlock = i - b * blockSize;
                    int blockEnd = Math.Min(last, (b + 1) * blockSize);
                    int n = blockEnd - i;
                    ref ushort rowPtr = ref Unsafe.Add(ref MemoryMarshal.GetArrayDataReference(train.Blocks[b]), rowInBlock * features);

                    for (int k = 0; k < n; k++)
                    {
                        double predicted = 0;
                        int f = 0;
                        for (; f + 4 <= features; f += 4)
                        {
                            int i0 = Unsafe.Add(ref baseRef, f) + Unsafe.Add(ref rowPtr, f);
                            int i1 = Unsafe.Add(ref baseRef, f + 1) + Unsafe.Add(ref rowPtr, f + 1);
                            int i2 = Unsafe.Add(ref baseRef, f + 2) + Unsafe.Add(ref rowPtr, f + 2);
                            int i3 = Unsafe.Add(ref baseRef, f + 3) + Unsafe.Add(ref rowPtr, f + 3);
                            Unsafe.Add(ref idxRef, f) = i0;
                            Unsafe.Add(ref idxRef, f + 1) = i1;
                            Unsafe.Add(ref idxRef, f + 2) = i2;
                            Unsafe.Add(ref idxRef, f + 3) = i3;
                            predicted += Unsafe.Add(ref wRef, i0) + Unsafe.Add(ref wRef, i1)
                                       + Unsafe.Add(ref wRef, i2) + Unsafe.Add(ref wRef, i3);
                        }
                        for (; f < features; f++)
                        {
                            int idx = Unsafe.Add(ref baseRef, f) + Unsafe.Add(ref rowPtr, f);
                            Unsafe.Add(ref idxRef, f) = idx;
                            predicted += Unsafe.Add(ref wRef, idx);
                        }

                        double residual = Unsafe.Add(ref scoreRef, i + k) - predicted;
                        squared += residual * residual;
                        absolute += Math.Abs(residual);
                        f = 0;
                        for (; f + 4 <= features; f += 4)
                        {
                            Unsafe.Add(ref sRef, Unsafe.Add(ref idxRef, f)) += residual;
                            Unsafe.Add(ref sRef, Unsafe.Add(ref idxRef, f + 1)) += residual;
                            Unsafe.Add(ref sRef, Unsafe.Add(ref idxRef, f + 2)) += residual;
                            Unsafe.Add(ref sRef, Unsafe.Add(ref idxRef, f + 3)) += residual;
                        }
                        for (; f < features; f++)
                        {
                            Unsafe.Add(ref sRef, Unsafe.Add(ref idxRef, f)) += residual;
                        }

                        rowPtr = ref Unsafe.Add(ref rowPtr, features);
                    }

                    i = blockEnd;
                }

                errors[2 * worker] = squared;
                errors[2 * worker + 1] = absolute;
            });

            double trainMae = Enumerable.Range(0, workers).Sum(w => errors[2 * w + 1]) / train.Count;

            // Adam on the mean residual of each weight; the prior pulls rarely seen weights towards zero.
            double correction1 = 1 - Math.Pow(Beta1, loop);
            double correction2 = 1 - Math.Pow(Beta2, loop);
            Parallel.For(0, workers, new ParallelOptions { MaxDegreeOfParallelism = workers }, worker =>
            {
                int first = count * worker / workers;
                int last = count * (worker + 1) / workers;
                for (int index = first; index < last; index++)
                {
                    if (appear[index] == 0)
                    {
                        continue;
                    }

                    double total = 0;
                    for (int w = 0; w < workers; w++)
                    {
                        total += partial[w][index];
                    }

                    double gradient = -(total - prior * weights[index]) / (appear[index] + prior);
                    m[index] = Beta1 * m[index] + (1 - Beta1) * gradient;
                    v[index] = Beta2 * v[index] + (1 - Beta2) * gradient * gradient;
                    weights[index] -= step * (m[index] / correction1) / (Math.Sqrt(v[index] / correction2) + Epsilon);
                }
            });

            (double mse, double mae) = Error(weights, validation, workers, errors);
            if (mse < bestMse)
            {
                bestMse = mse;
                bestMae = mae;
                bestTrainMae = trainMae;
                Array.Copy(weights, best, count);
                sinceBest = 0;
            }
            else if (++sinceBest >= patience)
            {
                step /= 2;
                sinceBest = 0;
                Array.Copy(best, weights, count);
            }
        }

        // Weights seen fewer than minAppear times are noise: zero them (they also compress better), and measure again.
        if (minAppear > 0)
        {
            for (int index = 0; index < count; index++)
            {
                if (appear[index] < minAppear)
                {
                    best[index] = 0;
                }
            }

            (bestMse, bestMae) = Error(best, validation, workers, errors);
        }

        return (best, new Fit(loop - 1, bestTrainMae, bestMae, bestMse));
    }

    private static (double Mse, double Mae) Error(double[] weights, Encoded data, int workers, double[] errors)
    {
        int activeWorkers = data.Count >= 10_000 ? workers : 1;
        Array.Clear(errors, 0, activeWorkers * 2);
        Parallel.For(0, activeWorkers, new ParallelOptions { MaxDegreeOfParallelism = activeWorkers }, worker =>
        {
            double squared = 0;
            double absolute = 0;
            int first = (int)((long)data.Count * worker / activeWorkers);
            int last = (int)((long)data.Count * (worker + 1) / activeWorkers);
            int features = data.Features;
            int[] featureBase = data.FeatureBase;
            ref int baseRef = ref MemoryMarshal.GetArrayDataReference(featureBase);
            ref double wRef = ref MemoryMarshal.GetArrayDataReference(weights);
            ref float scoreRef = ref MemoryMarshal.GetArrayDataReference(data.Scores);
            int blockSize = data.BlockSize;

            for (int i = first; i < last;)
            {
                int b = i / blockSize;
                int rowInBlock = i - b * blockSize;
                int blockEnd = Math.Min(last, (b + 1) * blockSize);
                int n = blockEnd - i;
                ref ushort rowPtr = ref Unsafe.Add(ref MemoryMarshal.GetArrayDataReference(data.Blocks[b]), rowInBlock * features);

                for (int k = 0; k < n; k++)
                {
                    double predicted = 0;
                    int f = 0;
                    for (; f + 4 <= features; f += 4)
                    {
                        predicted += Unsafe.Add(ref wRef, Unsafe.Add(ref baseRef, f) + Unsafe.Add(ref rowPtr, f))
                                   + Unsafe.Add(ref wRef, Unsafe.Add(ref baseRef, f + 1) + Unsafe.Add(ref rowPtr, f + 1))
                                   + Unsafe.Add(ref wRef, Unsafe.Add(ref baseRef, f + 2) + Unsafe.Add(ref rowPtr, f + 2))
                                   + Unsafe.Add(ref wRef, Unsafe.Add(ref baseRef, f + 3) + Unsafe.Add(ref rowPtr, f + 3));
                    }
                    for (; f < features; f++)
                    {
                        predicted += Unsafe.Add(ref wRef, Unsafe.Add(ref baseRef, f) + Unsafe.Add(ref rowPtr, f));
                    }

                    double residual = Unsafe.Add(ref scoreRef, i + k) - predicted;
                    squared += residual * residual;
                    absolute += Math.Abs(residual);
                    rowPtr = ref Unsafe.Add(ref rowPtr, features);
                }

                i = blockEnd;
            }

            errors[2 * worker] = squared;
            errors[2 * worker + 1] = absolute;
        });

        double totalSq = 0;
        double totalAbs = 0;
        for (int w = 0; w < activeWorkers; w++)
        {
            totalSq += errors[2 * w];
            totalAbs += errors[2 * w + 1];
        }

        return (totalSq / data.Count, totalAbs / data.Count);
    }
}

/// <summary>Numbers the weights of a pattern set after folding: one number per representative of every shape.</summary>
internal sealed class FoldedIndex
{
    private readonly int[] _ofRaw;
    private readonly int[] _rawOf;

    public FoldedIndex(PatternSet set)
    {
        Set = set;
        _ofRaw = new int[set.RawSize];
        var rawOf = new List<int>();
        var shapeStart = new int[set.Shapes.Count];
        for (int s = 0; s < set.Shapes.Count; s++)
        {
            PatternShape shape = set.Shapes[s];
            if (shape.Size > ushort.MaxValue + 1)
            {
                throw new ArgumentException($"A shape of {set.Name} has {shape.Size:N0} raw indices; the trainer stores at most 65,536 per shape.");
            }

            shapeStart[s] = rawOf.Count;
            var number = new Dictionary<int, int>();
            foreach (int representative in shape.Representatives)
            {
                number[representative] = rawOf.Count;
                rawOf.Add(set.Offsets[s] + representative);
            }

            for (int raw = 0; raw < shape.Size; raw++)
            {
                _ofRaw[set.Offsets[s] + raw] = number[shape.Fold[raw]];
            }
        }

        _rawOf = rawOf.ToArray();
        var shapes = set.Shapes.ToList();
        FeatureBase = Enumerable.Range(0, set.FeatureCount).Select(f => shapeStart[shapes.IndexOf(set.ShapeOf(f))]).ToArray();
    }

    /// <summary>The first folded number of each feature's shape: a feature's folded numbers follow it.</summary>
    public int[] FeatureBase { get; }

    public PatternSet Set { get; }

    public int Count => _rawOf.Length;

    /// <summary>The folded number of a raw index (plus its shape's offset).</summary>
    public int Of(int raw) => _ofRaw[raw];

    /// <summary>Folded weights in discs as a full table of raw indices in 1/<paramref name="units"/> discs.</summary>
    public short[] Expand(double[] weights, int units)
    {
        var table = new short[Set.RawSize];
        for (int raw = 0; raw < table.Length; raw++)
        {
            table[raw] = (short)Math.Clamp(Math.Round(weights[_ofRaw[raw]] * units, MidpointRounding.AwayFromZero), short.MinValue, short.MaxValue);
        }

        return table;
    }
}
