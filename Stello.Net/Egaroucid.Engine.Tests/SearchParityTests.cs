// SPDX-License-Identifier: GPL-3.0-only

using System.Globalization;
using System.Text;

namespace Stello.Egaroucid.Tests;

/// <summary>
/// Phase 3 of Plans/egaroucid-port-plan.md: the single-threaded search gives the same move, value, depth,
/// selectivity and number of nodes as the C++ Egaroucid (the search fixtures of tools/Egaroucid.Reference).
/// </summary>
/// <remarks>
/// The tests run the first positions of each fixture. Set EGAROUCID_FULL_PARITY=1 to run all of them (a few minutes).
/// The transposition table has the C++ size (2^25 + 2 slots, about 800 MB), which exact parity needs.
/// </remarks>
public class SearchParityTests
{
    private static readonly Lazy<SearchContext> Context = new(() =>
    {
        Fixtures.LoadResources();
        return new SearchContext(HashTables.DefaultHashLevel);
    });

    private static int PositionsPerFixture =>
        Environment.GetEnvironmentVariable("EGAROUCID_FULL_PARITY") == "1" ? int.MaxValue : 12;

    [Fact]
    public void MpcTable_EqualsReference()
    {
        Dictionary<string, string> rows = Fixtures.Rows("mpc.txt").ToDictionary(row => row[0], row => row[1]);
        Assert.Equal(MultiProbCut.MpcError.Length.ToString(CultureInfo.InvariantCulture), rows["entries"]);
        ulong checksum = 1469598103934665603UL;
        foreach (int value in MultiProbCut.MpcError)
        {
            for (int b = 0; b < 4; ++b)
            {
                checksum ^= ((uint)value >> (8 * b)) & 0xFF;
                checksum *= 1099511628211UL;
            }
        }

        Assert.Equal(rows["fnv1a"], checksum.ToString("x", CultureInfo.InvariantCulture));
    }

    [EgaroucidFact]
    public void Midgame_Level1_EqualsReference() => CheckFixture("search-midgame-level1.txt");

    [EgaroucidFact]
    public void Midgame_Level5_EqualsReference() => CheckFixture("search-midgame-level5.txt");

    [EgaroucidFact]
    public void Midgame_Level11_EqualsReference() => CheckFixture("search-midgame-level11.txt");

    [EgaroucidFact]
    public void Midgame_Level15_EqualsReference() => CheckFixture("search-midgame-level15.txt");

    [EgaroucidFact]
    public void Midgame_Level21_EqualsReference() => CheckFixture("search-midgame-level21.txt");

    [EgaroucidFact]
    public void Ffo_Level60_EqualsReference() => CheckFixture("search-ffo-level60.txt");

    private static void CheckFixture(string name)
    {
        SearchContext context = Context.Value;
        var failures = new StringBuilder();
        int checkedPositions = 0;
        foreach ((string[] row, List<string> expectedLog) in ReadSearchFixture(name).Take(PositionsPerFixture))
        {
            Board board = Board.FromString(row[1]);
            int level = int.Parse(row[2], CultureInfo.InvariantCulture);
            context.Clear();
            var log = new List<string>();
            SearchResult result = Ai.Search(context, board, level, useMultiThread: false, log.Add);
            string actual = string.Join('\t', Ai.PolicyName(result.Policy), result.Value, result.Depth, result.Probability,
                result.IsEndSearch ? 1 : 0, result.Nodes, result.ClogNodes);
            string expected = string.Join('\t', row[3..10]);
            if (actual != expected)
            {
                failures.AppendLine(CultureInfo.InvariantCulture, $"{row[0]}: expected {expected}, got {actual}");
                List<string> expectedIterations = expectedLog.Where(line => line.StartsWith("pre ", StringComparison.Ordinal) || line.StartsWith("main ", StringComparison.Ordinal)).ToList();
                for (int i = 0; i < Math.Max(expectedIterations.Count, log.Count); i++)
                {
                    string e = i < expectedIterations.Count ? expectedIterations[i] : "-";
                    string a = i < log.Count ? log[i] : "-";
                    failures.AppendLine(CultureInfo.InvariantCulture, $"    {(e == a ? "  " : "!=")} C++: {e}");
                    if (e != a)
                    {
                        failures.AppendLine(CultureInfo.InvariantCulture, $"       C#:  {a}");
                    }
                }
            }

            checkedPositions++;
        }

        Assert.True(failures.Length == 0, $"{name}: {checkedPositions} positions checked\n{failures}");
    }

    /// <summary>The result rows of a search fixture, each with the log lines (without "# ") that follow it.</summary>
    private static IEnumerable<(string[] Row, List<string> Log)> ReadSearchFixture(string name)
    {
        string[]? row = null;
        List<string> log = [];
        bool header = true;
        foreach (string line in File.ReadLines(Fixtures.PathOf(name)))
        {
            if (line.StartsWith('#'))
            {
                if (!header && row is not null)
                {
                    log.Add(line.Length > 2 ? line[2..] : "");
                }

                continue;
            }

            header = false;
            if (line.Length == 0)
            {
                continue;
            }

            if (row is not null)
            {
                yield return (row, log);
            }

            row = line.Split('\t');
            log = [];
        }

        if (row is not null)
        {
            yield return (row, log);
        }
    }
}
