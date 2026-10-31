using Stello.Engine.Benchmark;
using V1 = Stello.Engine.V1;

namespace Stello.Engine.Tests;

/// <summary>Stello 1 (Stello.Engine.V1) is the frozen baseline: it must keep searching exactly as recorded.</summary>
public class StelloV1Tests
{
    public static TheoryData<string> Signatures()
    {
        var data = new TheoryData<string>();
        foreach (string line in File.ReadLines(Path.Combine(AppContext.BaseDirectory, "Data", "stello-v1-signatures.txt")))
        {
            if (!line.StartsWith('#'))
            {
                data.Add(line);
            }
        }

        return data;
    }

    [Theory]
    [MemberData(nameof(Signatures))]
    public void Search_MatchesTheRecordedSignature(string signature)
    {
        string[] fields = signature.Split(' ');
        BenchmarkPosition position = BenchmarkPosition.Load(Path.Combine(AppContext.BaseDirectory, "Data", "Benchmark", fields[0]))
            .Single(p => p.Id == fields[1]);
        SearchLimits limits = fields[2] == "solve" ? SearchLimits.Solve : SearchLimits.FixedDepth(int.Parse(fields[2]["depth=".Length..]));
        var engine = new V1.SearchEngine();

        SearchResult result = engine.Search(position.Board, position.Player, limits);

        Assert.Equal(signature, string.Join(' ', fields[0], fields[1], fields[2],
            result.Move.ToString(), result.Score, result.Kind, result.Depth, result.Nodes));
    }
}
