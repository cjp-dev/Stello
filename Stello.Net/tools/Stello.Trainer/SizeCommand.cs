using System.IO.Compression;
using Stello.BookTool;
using Stello.Engine.Evaluation;

namespace Stello.Trainer;

/// <summary>size: how small a model's weights compress with coarser units and with stages stored as differences.</summary>
internal static class SizeCommand
{
    public static int Run(Options options)
    {
        options.Allow("model");
        EvalWeights model = EvalWeights.Load(options.Text("model") ?? throw new ArgumentException("size needs --model <file>."));
        PatternSet set = model.Set;
        Console.WriteLine($"{model.Stages} stages, {model.UnitsPerDisc} units per disc.");
        Console.WriteLine("| Units per disc | Plain | Stage differences | Zero weights |");
        Console.WriteLine("|---:|---:|---:|---:|");
        foreach (int units in new[] { 32, 16, 8, 4 })
        {
            var stages = new List<short[]>();
            long zeros = 0;
            long total = 0;
            for (int stage = 0; stage < model.Stages; stage++)
            {
                var folded = new List<short>();
                ReadOnlySpan<short> weights = model.StageWeights(stage);
                for (int s = 0; s < set.Shapes.Count; s++)
                {
                    foreach (int representative in set.Shapes[s].Representatives)
                    {
                        short value = (short)Math.Round(weights[set.Offsets[s] + representative] * (double)units / model.UnitsPerDisc, MidpointRounding.AwayFromZero);
                        folded.Add(value);
                        zeros += value == 0 ? 1 : 0;
                        total++;
                    }
                }

                stages.Add(folded.ToArray());
            }

            long plain = Gzip(stages);
            var differences = stages.Select((weights, stage) => stage == 0 ? weights
                : weights.Zip(stages[stage - 1], (a, b) => (short)(a - b)).ToArray()).ToList();
            Console.WriteLine($"| {units} | {plain:N0} | {Gzip(differences):N0} | {100.0 * zeros / total:0.0} % |");
        }

        return 0;
    }

    private static long Gzip(List<short[]> stages)
    {
        using var memory = new MemoryStream();
        using (var gzip = new GZipStream(memory, CompressionLevel.SmallestSize, leaveOpen: true))
        {
            foreach (short[] stage in stages)
            {
                // Low bytes of all weights, then high bytes: the high bytes are mostly 0 or 0xFF and compress well.
                gzip.Write(stage.Select(w => (byte)w).ToArray());
                gzip.Write(stage.Select(w => (byte)(w >> 8)).ToArray());
            }
        }

        return memory.Length;
    }
}
