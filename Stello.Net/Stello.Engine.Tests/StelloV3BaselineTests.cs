using Stello.Engine.Benchmark;

namespace Stello.Engine.Tests;

/// <summary>The current Stello v3 one-thread baseline, independent of the frozen Stello 2.0 engine.</summary>
public class StelloV3BaselineTests
{
    public static TheoryData<string> Signatures()
    {
        var data = new TheoryData<string>();
        foreach (string line in File.ReadLines(Path.Combine(AppContext.BaseDirectory, "Data", "stello-v3-signatures.txt")))
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
    public void CurrentEngine_MatchesTheRecordedSignature(string signature) =>
        Assert.Equal(signature, Search(signature));

    private static string Search(string signature)
    {
        string[] fields = signature.Split(' ');
        BenchmarkPosition position = BenchmarkPosition.Load(Path.Combine(AppContext.BaseDirectory, "Data", "Benchmark", fields[0]))
            .Single(p => p.Id == fields[1]);
        SearchLimits limits = fields[2] == "solve" ? SearchLimits.Solve : SearchLimits.FixedDepth(int.Parse(fields[2]["depth=".Length..]));

        SearchResult result = new SearchEngine().Search(position.Board, position.Player, limits);

        return string.Join(' ', fields[0], fields[1], fields[2], result.Move.ToString(), result.Score, result.Kind, result.Depth, result.Nodes);
    }
}
