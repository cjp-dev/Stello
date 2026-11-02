using Stello.Engine.Benchmark;
using V2 = Stello.Engine.V2;

namespace Stello.Engine.Tests;

/// <summary>
/// Stello 2.0 (Stello.Engine.V2) is the frozen baseline of Stello 3.0: it must keep searching exactly as recorded. The
/// engine being developed (Stello.Engine) must search exactly so too, as long as Part 1 of the 3.0 plan keeps it a pure
/// port (Plans/making-stello-3-plan.md).
/// </summary>
public class StelloV2Tests
{
    public static TheoryData<string> Signatures()
    {
        var data = new TheoryData<string>();
        foreach (string line in File.ReadLines(Path.Combine(AppContext.BaseDirectory, "Data", "stello-v2-signatures.txt")))
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
    public void FrozenEngine_MatchesTheRecordedSignature(string signature) =>
        Assert.Equal(signature, Search(signature, (board, player, limits) => new V2.SearchEngine().Search(board, player, limits)));

    [Theory]
    [MemberData(nameof(Signatures))]
    public void CurrentEngine_MatchesTheRecordedSignature(string signature) =>
        Assert.Equal(signature, Search(signature, (board, player, limits) => new SearchEngine().Search(board, player, limits)));

    private static string Search(string signature, Func<Board, Player, SearchLimits, SearchResult> search)
    {
        string[] fields = signature.Split(' ');
        BenchmarkPosition position = BenchmarkPosition.Load(Path.Combine(AppContext.BaseDirectory, "Data", "Benchmark", fields[0]))
            .Single(p => p.Id == fields[1]);
        SearchLimits limits = fields[2] == "solve" ? SearchLimits.Solve : SearchLimits.FixedDepth(int.Parse(fields[2]["depth=".Length..]));

        SearchResult result = search(position.Board, position.Player, limits);

        return string.Join(' ', fields[0], fields[1], fields[2], result.Move.ToString(), result.Score, result.Kind, result.Depth, result.Nodes);
    }
}
