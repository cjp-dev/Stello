// SPDX-License-Identifier: GPL-3.0-only

using System.Text.Json;
using System.Text.Json.Nodes;
using Stello.Benchmark;
using Stello.BookTool;
using Stello.Engine;

namespace Stello.Egaroucid.Tests;

public class MatchResumeTests
{
    [Fact]
    public void Match_ResumesAfterOneGameAndWritesTheSameReportAsAnUninterruptedRun()
    {
        using var temp = new TemporaryDirectory();
        string uninterruptedFolder = Path.Combine(temp.Path, "uninterrupted");
        string resumedFolder = Path.Combine(temp.Path, "resumed");
        Directory.CreateDirectory(uninterruptedFolder);
        Directory.CreateDirectory(resumedFolder);

        string sideA = "engine=stello-v1,depth=1,name=side-a";
        string sideB = "engine=stello-v1,depth=1,name=side-b";
        string name = "resume-test";
        List<string> starts = MatchCommands.LoadStarts(null).Take(1).ToList();
        MatchRunSettings settings = MatchRunSettings.Create(sideA, sideB, starts);
        string gamesPath = Path.Combine(resumedFolder, $"match_{name}.games.jsonl");
        MatchSide a = MatchCommands.ParseSide(sideA);
        MatchSide b = MatchCommands.ParseSide(sideB);
        using (MatchJournal journal = MatchJournal.Create(gamesPath, settings, starts))
        using (var cancellation = new CancellationTokenSource())
        {
            int savedGames = 0;
            Assert.Throws<OperationCanceledException>(() => new EngineMatch(a, b, 1).Play(
                starts,
                null,
                cancellation.Token,
                gamePlayed: (startIndex, second, game) =>
                {
                    journal.Append(new MatchGameRecord(
                        MatchJournal.FormatVersion, settings, startIndex, second ? 1 : 0, starts[startIndex], MatchGameData.From(game)));
                    if (++savedGames == 1)
                    {
                        cancellation.Cancel();
                    }
                }));
            Assert.Equal(1, savedGames);
        }

        Assert.Single(File.ReadAllLines(gamesPath));
        Assert.Equal(0, MatchCommands.Match(Options.Parse(
            ["--a", sideA, "--b", sideB, "--max-starts", "1", "--workers", "1", "--name", name]),
            uninterruptedFolder, Program.Cancellable));
        Assert.Equal(0, MatchCommands.Match(Options.Parse(
            ["--a", sideA, "--b", sideB, "--max-starts", "1", "--workers", "2", "--name", name, "--resume"], "resume"),
            resumedFolder, Program.Cancellable));

        string reportPath = $"match_{name}";
        string uninterruptedMarkdown = File.ReadAllText(Path.Combine(uninterruptedFolder, reportPath + ".md"));
        string resumedMarkdown = File.ReadAllText(Path.Combine(resumedFolder, reportPath + ".md"));
        Assert.Equal(NormalizeMarkdown(uninterruptedMarkdown), NormalizeMarkdown(resumedMarkdown));
        Assert.Equal(NormalizeJson(Path.Combine(uninterruptedFolder, reportPath + ".json")),
            NormalizeJson(Path.Combine(resumedFolder, reportPath + ".json")));
        Assert.Equal(2, File.ReadAllLines(gamesPath).Length);

        InvalidDataException exception = Assert.Throws<InvalidDataException>(() => MatchCommands.Match(Options.Parse(
            ["--a", "engine=stello-v1,depth=2,name=side-a", "--b", sideB, "--max-starts", "1", "--workers", "1", "--name", name, "--resume"], "resume"),
            resumedFolder, _ => throw new InvalidOperationException("The mismatched resume reached the match runner.")));
        Assert.Contains("side A or B settings, time limits, or Egaroucid book seeds", exception.Message, StringComparison.Ordinal);
        Assert.Throws<ArgumentException>(() => MatchCommands.Match(Options.Parse(
            ["--a", sideA, "--b", sideB, "--max-starts", "1", "--workers", "1", "--name", name]),
            resumedFolder, _ => throw new InvalidOperationException("A fresh run overwrote an existing match log.")));
    }

    [Theory]
    [InlineData("depth=2", "side A or B settings, time limits, or Egaroucid book seeds")]
    [InlineData("engine=egaroucid,time-ms=1,seed=2", "side A or B settings, time limits, or Egaroucid book seeds")]
    public void MatchJournal_ResumeRejectsChangedSideLimitsAndSeeds(string changedSide, string expectedMessage)
    {
        using var temp = new TemporaryDirectory();
        string path = Path.Combine(temp.Path, "match.games.jsonl");
        string start = "d3";
        string originalSideA = "engine=egaroucid,time-ms=1,seed=1";
        string sideB = "depth=1";
        List<string> starts = [start];
        MatchRunSettings saved = MatchRunSettings.Create(originalSideA, sideB, starts);
        using (MatchJournal journal = MatchJournal.Create(path, saved, starts))
        {
            journal.Append(new MatchGameRecord(
                MatchJournal.FormatVersion, saved, 0, 0, start, new MatchGameData(Player.Black, 0, default, default)));
        }

        MatchRunSettings requested = MatchRunSettings.Create(changedSide, sideB, starts);
        InvalidDataException exception = Assert.Throws<InvalidDataException>(() => MatchJournal.Resume(path, requested, starts));

        Assert.Contains(expectedMessage, exception.ToString(), StringComparison.Ordinal);
    }

    [Fact]
    public void MatchJournal_ResumeRejectsChangedStartPositions()
    {
        using var temp = new TemporaryDirectory();
        string path = Path.Combine(temp.Path, "match.games.jsonl");
        List<string> savedStarts = ["d3"];
        MatchRunSettings saved = MatchRunSettings.Create("depth=1", "depth=1", savedStarts);
        using (MatchJournal journal = MatchJournal.Create(path, saved, savedStarts))
        {
            journal.Append(new MatchGameRecord(
                MatchJournal.FormatVersion, saved, 0, 0, "d3", new MatchGameData(Player.Black, 0, default, default)));
        }

        List<string> requestedStarts = ["d3c3"];
        MatchRunSettings requested = MatchRunSettings.Create("depth=1", "depth=1", requestedStarts);
        InvalidDataException exception = Assert.Throws<InvalidDataException>(() => MatchJournal.Resume(path, requested, requestedStarts));

        Assert.Contains("start positions differ", exception.ToString(), StringComparison.Ordinal);
    }

    [Fact]
    public void MatchJournal_ResumeDropsOnlyAnIncompleteFinalLine()
    {
        using var temp = new TemporaryDirectory();
        string path = Path.Combine(temp.Path, "match.games.jsonl");
        List<string> starts = ["d3"];
        MatchRunSettings settings = MatchRunSettings.Create("depth=1", "depth=1", starts);
        using (MatchJournal journal = MatchJournal.Create(path, settings, starts))
        {
            journal.Append(new MatchGameRecord(
                MatchJournal.FormatVersion, settings, 0, 0, "d3", new MatchGameData(Player.Black, 0, default, default)));
        }

        using (var stream = new FileStream(path, FileMode.Append, FileAccess.Write, FileShare.Read))
        {
            stream.Write("{ incomplete"u8);
        }

        using (MatchJournal resumed = MatchJournal.Resume(path, settings, starts))
        {
            Assert.Single(resumed.AFirstGames());
        }

        Assert.Single(File.ReadAllLines(path));
    }

    [Fact]
    public void MatchJournal_ResumeRefusesAnEmptyLog()
    {
        using var temp = new TemporaryDirectory();
        string path = Path.Combine(temp.Path, "match.games.jsonl");
        List<string> starts = ["d3"];
        MatchRunSettings settings = MatchRunSettings.Create("depth=1", "depth=1", starts);
        using (MatchJournal journal = MatchJournal.Create(path, settings, starts))
        {
        }

        ArgumentException exception = Assert.Throws<ArgumentException>(() => MatchJournal.Resume(path, settings, starts));

        Assert.Contains("contains no completed games", exception.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void MatchReport_SeparatesOrdinaryDiscScoreFromFlagAdjustedOverrunScore()
    {
        var a = new MatchSide("base", SearchLimits.TimePerGame(TimeSpan.FromSeconds(15), useOpeningBook: false), HashBits: 16);
        var b = new MatchSide("cand", SearchLimits.TimePerGame(TimeSpan.FromSeconds(15), useOpeningBook: false), HashBits: 16);
        var aFirst = new MatchGame(Player.White, [], DiscsB: 4)
        {
            ElapsedA = TimeSpan.FromSeconds(10),
            ElapsedB = TimeSpan.FromSeconds(16),
            FirstOverrun = ClockOverrunSide.B,
        };
        var bFirst = new MatchGame(Player.Black, [], DiscsB: 4)
        {
            ElapsedA = TimeSpan.FromSeconds(11),
            ElapsedB = TimeSpan.FromSeconds(12),
            FirstOverrun = ClockOverrunSide.None,
        };
        var report = MatchReport.Of(a, b, [new MatchPair("d3", aFirst, bFirst)], TimeSpan.FromSeconds(2));

        Assert.True(report.HasGameClock);
        Assert.Equal(0, report.OverrunsA);
        Assert.Equal(1, report.OverrunsB);
        Assert.Equal(2.0, report.PointsB);
        Assert.Equal(1.0, report.Score, 6);
        Assert.Equal(1.0, report.FlagAdjustedPointsB);
        Assert.Equal(0.5, report.FlagAdjustedScore, 6);
        Assert.Equal(0.0, report.FlagAdjustedElo, 6);

        string md = report.Markdown(true);
        Assert.Contains("| Clock overruns (first to exceed) | 0 | 1 |", md, StringComparison.Ordinal);
        Assert.Contains("B scored **100.0 %**", md, StringComparison.Ordinal);
        Assert.Contains("Flag-adjusted (first clock overrun loses): B **1/2** (**50.0 %**", md, StringComparison.Ordinal);
    }

    [Fact]
    public void MatchJournal_RoundTripsClockOverrunsAndLoadsLegacyV1RecordsWithoutThem()
    {
        using var temp = new TemporaryDirectory();
        string path = Path.Combine(temp.Path, "match.games.jsonl");
        List<string> starts = ["d3"];
        MatchRunSettings settings = MatchRunSettings.Create("game-s=15", "game-s=15", starts);

        string settingsJson = JsonSerializer.Serialize(settings);
        string legacyLine = "{\"Version\":1,\"Settings\":" + settingsJson
            + ",\"StartIndex\":0,\"GameNumber\":0,\"Start\":\"d3\",\"Game\":{\"ColourB\":0,\"DiscsB\":2,\"StatsA\":{},\"StatsB\":{}}}";
        File.WriteAllText(path, legacyLine + "\n");

        using (MatchJournal journal = MatchJournal.Resume(path, settings, starts))
        {
            journal.Append(new MatchGameRecord(
                MatchJournal.FormatVersion,
                settings,
                0,
                1,
                "d3",
                MatchGameData.From(new MatchGame(Player.White, [], DiscsB: 6)
                {
                    ElapsedA = TimeSpan.FromSeconds(16),
                    ElapsedB = TimeSpan.FromSeconds(14),
                    FirstOverrun = ClockOverrunSide.A,
                })));
        }

        using MatchJournal reloaded = MatchJournal.Resume(path, settings, starts);
        MatchPair pair = Assert.Single(reloaded.Pairs());
        Assert.Equal(TimeSpan.Zero, pair.AFirst.ElapsedA);
        Assert.Equal(ClockOverrunSide.None, pair.AFirst.FirstOverrun);
        Assert.Equal(TimeSpan.FromSeconds(16), pair.BFirst.ElapsedA);
        Assert.Equal(TimeSpan.FromSeconds(14), pair.BFirst.ElapsedB);
        Assert.Equal(ClockOverrunSide.A, pair.BFirst.FirstOverrun);
    }

    private static string NormalizeMarkdown(string markdown)
    {
        string normalized = markdown;
        foreach (string row in new[] { "Nodes per second", "Thinking time per game", "Time per searched move (average / longest)" })
        {
            normalized = System.Text.RegularExpressions.Regex.Replace(
                normalized, $@"(?m)^\| {System.Text.RegularExpressions.Regex.Escape(row)} \|.*$", $"| {row} | <elapsed> | <elapsed> |");
        }

        return System.Text.RegularExpressions.Regex.Replace(
            normalized, @"Wall time [^.]+\.", "Wall time <elapsed>.", System.Text.RegularExpressions.RegexOptions.CultureInvariant);
    }

    private static string NormalizeJson(string path)
    {
        JsonNode json = JsonNode.Parse(File.ReadAllText(path))!;
        json["Duration"] = "<elapsed>";
        foreach (string side in new[] { "A", "B" })
        {
            JsonObject summary = (JsonObject)json[side]!;
            summary.Remove("NodesPerSecond");
            summary.Remove("SecondsPerGame");
            summary.Remove("MsPerSearchedMove");
            summary.Remove("LongestMoveMs");
        }
        if (json["Pairs"] is JsonArray pairs)
        {
            foreach (JsonObject pair in pairs.OfType<JsonObject>())
            {
                pair.Remove("ElapsedSecondsAWhenAFirst");
                pair.Remove("ElapsedSecondsBWhenAFirst");
                pair.Remove("ElapsedSecondsAWhenBFirst");
                pair.Remove("ElapsedSecondsBWhenBFirst");
            }
        }

        return json.ToJsonString(new JsonSerializerOptions { WriteIndented = true });
    }

    private sealed class TemporaryDirectory : IDisposable
    {
        private readonly string _path = Directory.CreateTempSubdirectory("Stello.MatchResumeTests.").FullName;

        public string Path => _path;

        public void Dispose() => Directory.Delete(_path, recursive: true);
    }
}
