// SPDX-License-Identifier: GPL-3.0-only

using System.Runtime.CompilerServices;
using System.Security.Cryptography;
using System.Text.Json;
using Stello.Benchmark;
using Stello.BookTool;
using Stello.Engine;
using Stello.Engine.Benchmark;
using EngineBoard = Stello.Engine.Board;
using EngineSearchResult = Stello.Engine.SearchResult;

namespace Stello.Egaroucid.Tests;

[Collection("Timing")]
public sealed class StelloLibraryMatchTests(ReferenceLibraryFixture fixture) : IClassFixture<ReferenceLibraryFixture>
{
    [Theory]
    [InlineData("engine=stello-lib,depth=1", "needs lib and sha256")]
    [InlineData("depth=1,lib=ignored", "only for engine=stello-lib")]
    [InlineData("engine=stello-lib,depth=1,lib=ignored,sha256=ignored,book=on", "no book or search switches")]
    [InlineData("engine=stello-lib,depth=1,lib=ignored,sha256=ignored,opt=ybwc=off", "no book or search switches")]
    [InlineData("engine=stello-lib,depth=1,lib=a,lib=b,sha256=ignored", "given more than once")]
    [InlineData("engine=stello-lib,depth=1,lib=ignored,sha256=a,sha256=b", "given more than once")]
    public void ParseSide_RejectsInvalidLibraryOptions(string text, string message)
    {
        ArgumentException exception = Assert.Throws<ArgumentException>(() => MatchCommands.ParseSide(text));

        Assert.Contains(message, exception.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void ParseSide_RejectsWrongHashAndTheCurrentLibraryName()
    {
        ArgumentException wrongHash = Assert.Throws<ArgumentException>(() =>
            MatchCommands.ParseSide($"engine=stello-lib,depth=1,lib={fixture.AliasPath},sha256={new string('0', 64)}"));
        ArgumentException currentName = Assert.Throws<ArgumentException>(() =>
            MatchCommands.ParseSide($"engine=stello-lib,depth=1,lib={fixture.OriginalPath},sha256={fixture.Sha256}"));

        Assert.Contains("SHA-256", wrongHash.Message, StringComparison.Ordinal);
        Assert.Contains("different file name", currentName.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void ParseSide_RejectsAnInvalidNativeLibrary()
    {
        string path = System.IO.Path.Combine(fixture.Folder, "not-a-library.dll");
        File.WriteAllText(path, "not a native library");
        string hash = ReferenceLibraryFixture.Hash(path);

        ArgumentException exception = Assert.Throws<ArgumentException>(() =>
            MatchCommands.ParseSide($"engine=stello-lib,depth=1,lib={path},sha256={hash}"));

        Assert.Contains("Cannot load Stello library", exception.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void ParseSide_RechecksTheLibraryHashWhenAWorkerStarts()
    {
        string path = System.IO.Path.Combine(fixture.Folder, "changed-reference.dll");
        File.Copy(fixture.AliasPath, path);
        MatchSide side = MatchCommands.ParseSide($"engine=stello-lib,depth=1,lib={path},sha256={fixture.Sha256}");
        File.AppendAllText(path, "changed");

        ArgumentException exception = Assert.Throws<ArgumentException>(() => side.NewPlayer());

        Assert.Contains("SHA-256", exception.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void BenchSearcher_RejectsLibrarySidesInsteadOfUsingTheCurrentEngine()
    {
        Options options = Options.Parse(["--side", "engine=stello-lib"]);

        ArgumentException exception = Assert.Throws<ArgumentException>(() =>
            BenchSearcher.Create(options, SearchLimits.FixedDepth(2), "depth 2"));

        Assert.Contains("only supported by match", exception.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void MidQuality_RejectsLibrarySidesInsteadOfIgnoringTheirSettings()
    {
        Options options = Options.Parse(["--sides", "engine=stello-lib", "--max-positions", "1"]);

        ArgumentException exception = Assert.Throws<ArgumentException>(() => QualityCommands.MidQuality(options));

        Assert.Contains("only supported by match", exception.Message, StringComparison.Ordinal);
    }

    [Theory]
    [InlineData(true)]
    [InlineData(false)]
    public void Match_AcceptsTheLibraryOnEitherSideWithoutStartingAGame(bool libraryIsA)
    {
        string librarySide = SideText("time-ms=100,hash-bits=10,threads=1,name=reference");
        string currentSide = "time-ms=100,hash-bits=10,threads=1,name=current";
        string name = libraryIsA ? "library-a" : "library-b";
        Options options = Options.Parse([
            "--a", libraryIsA ? librarySide : currentSide,
            "--b", libraryIsA ? currentSide : librarySide,
            "--workers", "12", "--max-starts", "1", "--name", name,
        ]);
        bool runnerCalled = false;

        int status = MatchCommands.Match(options, fixture.Folder, _ =>
        {
            runnerCalled = true;
            return false;
        });

        Assert.True(runnerCalled);
        Assert.Equal(2, status);
        Assert.Empty(File.ReadAllLines(System.IO.Path.Combine(fixture.Folder, $"match_{name}.games.jsonl")));
    }

    [Theory]
    [InlineData(true)]
    [InlineData(false)]
    public void MatchResume_RejectsAChangedReferenceBeforeStartingAGame(bool changePath)
    {
        string suffix = changePath ? "path" : "bytes";
        string original = System.IO.Path.Combine(fixture.Folder, $"resume-reference-{suffix}.dll");
        File.Copy(fixture.AliasPath, original);
        string sideA = "time-ms=100,name=current";
        string sideB = $"engine=stello-lib,lib={original},sha256={fixture.Sha256},time-ms=100,name=reference";
        List<string> starts = MatchCommands.LoadStarts(null).Take(1).ToList();
        MatchRunSettings settings = MatchRunSettings.Create(sideA, sideB, starts);
        string name = $"resume-reference-{suffix}";
        string gamesPath = System.IO.Path.Combine(fixture.Folder, $"match_{name}.games.jsonl");
        using (MatchJournal journal = MatchJournal.Create(gamesPath, settings, starts))
        {
            Player colourB = BookTextFormat.Replay(starts[0]).Player.Opponent();
            journal.Append(new MatchGameRecord(
                MatchJournal.FormatVersion, settings, 0, 0, starts[0], new MatchGameData(colourB, 0, default, default)));
        }

        if (changePath)
        {
            string moved = System.IO.Path.Combine(fixture.Folder, "resume-reference-other.dll");
            File.Copy(original, moved);
            sideB = $"engine=stello-lib,lib={moved},sha256={fixture.Sha256},time-ms=100,name=reference";
        }
        else
        {
            File.AppendAllText(original, "changed");
        }

        Options options = Options.Parse([
            "--a", sideA, "--b", sideB, "--max-starts", "1", "--workers", "1", "--name", name, "--resume",
        ], "resume");
        bool runnerCalled = false;
        int Run() => MatchCommands.Match(options, fixture.Folder, _ =>
        {
            runnerCalled = true;
            return false;
        });

        Exception exception = changePath
            ? Assert.Throws<InvalidDataException>(() => Run())
            : Assert.Throws<ArgumentException>(() => Run());
        Assert.Contains(changePath ? "side A or B settings" : "SHA-256", exception.Message, StringComparison.Ordinal);
        Assert.False(runnerCalled);
        Assert.Single(File.ReadAllLines(gamesPath));
    }

    [Fact]
    public void LibraryPlayer_MatchesTheCurrentEngineAndClearsItsHash()
    {
        MatchSide side = MatchCommands.ParseSide(SideText("depth=6,hash-bits=10"));
        Assert.Contains($"SHA-256 {fixture.Sha256}", MatchCommands.Describe(side), StringComparison.Ordinal);
        using var player = Assert.IsType<StelloLibraryMatchPlayer>(side.NewPlayer());
        var limits = SearchLimits.FixedDepth(6, useOpeningBook: false, hashBits: 10);
        var current = new SearchEngine(10);

        Assert.Throws<InvalidOperationException>(() => player.ChooseMove(EngineBoard.Initial, Player.Black, limits, CancellationToken.None));
        player.NewGame();
        EngineSearchResult first = player.ChooseMove(EngineBoard.Initial, Player.Black, limits, CancellationToken.None);
        EngineSearchResult expected = current.Search(EngineBoard.Initial, Player.Black, limits);
        player.NewGame();
        EngineSearchResult afterReset = player.ChooseMove(EngineBoard.Initial, Player.Black, limits, CancellationToken.None);

        AssertSearchEqual(expected, first);
        AssertSearchEqual(first, afterReset);
    }

    [Fact]
    public void LibraryPlayer_SupportsEightThreads()
    {
        MatchSide side = MatchCommands.ParseSide(SideText("depth=5,hash-bits=10,threads=8"));
        using var player = Assert.IsType<StelloLibraryMatchPlayer>(side.NewPlayer());
        player.NewGame();

        EngineSearchResult result = player.ChooseMove(EngineBoard.Initial, Player.Black, side.Limits, CancellationToken.None);

        Assert.Equal(8, side.Limits.Threads);
        Assert.NotNull(result.Move.Square);
        Assert.True(EngineBoard.Initial.IsLegal(Player.Black, result.Move.Square.Value));
        Assert.True(result.Nodes > 0);
    }

    [Fact]
    public void LibraryPlayer_CancelsAndCanSearchAgain()
    {
        MatchSide side = MatchCommands.ParseSide(SideText("depth=30,hash-bits=10"));
        using var player = Assert.IsType<StelloLibraryMatchPlayer>(side.NewPlayer());
        player.NewGame();
        using var cancellation = new CancellationTokenSource(TimeSpan.FromMilliseconds(30));

        Assert.Throws<OperationCanceledException>(() =>
            player.ChooseMove(EngineBoard.Initial, Player.Black, side.Limits, cancellation.Token));

        player.NewGame();
        EngineSearchResult result = player.ChooseMove(
            EngineBoard.Initial, Player.Black, SearchLimits.FixedDepth(3, useOpeningBook: false), CancellationToken.None);
        Assert.NotNull(result.Move.Square);
        Assert.True(EngineBoard.Initial.IsLegal(Player.Black, result.Move.Square.Value));
    }

    [Fact]
    public void LibraryPlayer_ReleasesNativeHandlesWhenCollected()
    {
        string path = System.IO.Path.Combine(fixture.Folder, "collected-reference" + System.IO.Path.GetExtension(fixture.OriginalPath));
        File.Copy(fixture.AliasPath, path);
        WeakReference player = SearchWithoutDisposing(path, fixture.Sha256);

        for (int i = 0; i < 3; i++)
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
        }

        Assert.False(player.IsAlive);
        File.Delete(path);
        Assert.False(File.Exists(path));
    }

    [StelloV30Fact]
    public void TaggedV30_MatchesTheHistoricBaselineWhileTheCurrentEngineRemainsDistinct()
    {
        string path = Environment.GetEnvironmentVariable("STELLO_V30_LIBRARY")!;
        string hash = ReferenceLibraryFixture.Hash(path);
        using (StelloLibrary library = StelloLibrary.Open(path, hash))
        {
            Assert.Equal(30_000u, library.Version);
        }

        MatchSide side = MatchCommands.ParseSide($"engine=stello-lib,lib={path},sha256={hash},depth=10,hash-bits=19");
        using var reference = Assert.IsType<StelloLibraryMatchPlayer>(side.NewPlayer());
        var current = new SearchEngine(19);
        var limits = SearchLimits.FixedDepth(10, useOpeningBook: false, hashBits: 19);
        List<BenchmarkPosition> positions = BenchmarkPosition.Load(Program.DataFile("midgame.txt"));
        string baselinePath = System.IO.Path.Combine(Program.RepositoryFolder(), "Plans", "benchmark-results", "v3.0-ref", "midgame-depth10.json");
        using JsonDocument baseline = JsonDocument.Parse(File.ReadAllText(baselinePath));
        Dictionary<string, JsonElement> expected = baseline.RootElement.GetProperty("Results").EnumerateArray()
            .ToDictionary(item => item.GetProperty("Id").GetString()!, StringComparer.Ordinal);
        int different = 0;

        Assert.Equal(100, positions.Count);
        Assert.Equal(positions.Count, expected.Count);
        foreach (BenchmarkPosition position in positions)
        {
            reference.NewGame();
            EngineSearchResult oldResult = reference.ChooseMove(position.Board, position.Player, limits, CancellationToken.None);
            AssertMatchesBaseline(expected[position.Id], oldResult);

            current.ClearHash();
            EngineSearchResult currentResult = current.Search(position.Board, position.Player, limits);
            if (currentResult.Nodes != oldResult.Nodes || currentResult.Move != oldResult.Move || currentResult.Score != oldResult.Score)
            {
                different++;
            }
        }

        Assert.True(different > 0, "The current engine must not be resolved to the loaded 3.0 library.");
    }

    [StelloV30Fact]
    public void TaggedV30_MatchesTheHistoricFfo40Through44()
    {
        string path = Environment.GetEnvironmentVariable("STELLO_V30_LIBRARY")!;
        string hash = ReferenceLibraryFixture.Hash(path);
        MatchSide side = MatchCommands.ParseSide($"engine=stello-lib,lib={path},sha256={hash},depth=1,hash-bits=19");
        using var reference = Assert.IsType<StelloLibraryMatchPlayer>(side.NewPlayer());
        List<BenchmarkPosition> positions = BenchmarkPosition.Load(Program.DataFile("ffo-40-59.txt")).Take(5).ToList();
        string baselinePath = System.IO.Path.Combine(Program.RepositoryFolder(), "Plans", "benchmark-results", "v3.0-ref", "ffo-40-49.json");
        using JsonDocument baseline = JsonDocument.Parse(File.ReadAllText(baselinePath));
        Dictionary<string, JsonElement> expected = baseline.RootElement.GetProperty("Results").EnumerateArray()
            .ToDictionary(item => item.GetProperty("Id").GetString()!, StringComparer.Ordinal);

        Assert.Equal(new[] { "40", "41", "42", "43", "44" }, positions.Select(position => position.Id));
        foreach (BenchmarkPosition position in positions)
        {
            reference.NewGame();
            EngineSearchResult result = reference.ChooseMove(position.Board, position.Player, SearchLimits.Solve, CancellationToken.None);
            AssertMatchesBaseline(expected[position.Id], result);
        }
    }

    [StelloV30Fact]
    public async Task TaggedV30_CanSearchConcurrentlyWithTheCurrentEngine()
    {
        string path = Environment.GetEnvironmentVariable("STELLO_V30_LIBRARY")!;
        string hash = ReferenceLibraryFixture.Hash(path);
        MatchSide side = MatchCommands.ParseSide($"engine=stello-lib,lib={path},sha256={hash},depth=10,hash-bits=19");
        using var reference = Assert.IsType<StelloLibraryMatchPlayer>(side.NewPlayer());
        var current = new SearchEngine(19);
        var limits = SearchLimits.FixedDepth(10, useOpeningBook: false, hashBits: 19);
        BenchmarkPosition position = BenchmarkPosition.Load(Program.DataFile("midgame.txt"))[0];
        reference.NewGame();

        Task<EngineSearchResult> oldTask = Task.Run(() =>
            reference.ChooseMove(position.Board, position.Player, limits, CancellationToken.None));
        Task<EngineSearchResult> currentTask = Task.Run(() => current.Search(position.Board, position.Player, limits));
        await Task.WhenAll(oldTask, currentTask);

        Assert.Equal(37_483, oldTask.Result.Nodes);
        Assert.Equal("c4", oldTask.Result.Move.ToString());
        Assert.NotNull(currentTask.Result.Move.Square);
        Assert.True(position.Board.IsLegal(position.Player, currentTask.Result.Move.Square.Value));
    }

    [StelloV30Fact]
    public async Task TaggedV30_AndCurrentCanSearchWithEightThreadsConcurrently()
    {
        string path = Environment.GetEnvironmentVariable("STELLO_V30_LIBRARY")!;
        string hash = ReferenceLibraryFixture.Hash(path);
        MatchSide side = MatchCommands.ParseSide($"engine=stello-lib,lib={path},sha256={hash},depth=6,hash-bits=10,threads=8");
        using var reference = Assert.IsType<StelloLibraryMatchPlayer>(side.NewPlayer());
        var current = new SearchEngine(10);
        using var gate = new Barrier(2);
        reference.NewGame();

        Task<EngineSearchResult> oldTask = StartSearch(() =>
            reference.ChooseMove(EngineBoard.Initial, Player.Black, side.Limits, CancellationToken.None));
        Task<EngineSearchResult> currentTask = StartSearch(() =>
            current.Search(EngineBoard.Initial, Player.Black, side.Limits));
        EngineSearchResult[] results = await Task.WhenAll(oldTask, currentTask);

        foreach (EngineSearchResult result in results)
        {
            Assert.NotNull(result.Move.Square);
            Assert.True(EngineBoard.Initial.IsLegal(Player.Black, result.Move.Square.Value));
            Assert.True(result.Nodes > 0);
        }

        Task<EngineSearchResult> StartSearch(Func<EngineSearchResult> search) => Task.Factory.StartNew(() =>
        {
            if (!gate.SignalAndWait(TimeSpan.FromSeconds(15)))
            {
                throw new TimeoutException("Both engine libraries did not start.");
            }

            return search();
        }, CancellationToken.None, TaskCreationOptions.LongRunning, TaskScheduler.Default);
    }

    [StelloV30Fact]
    public void TaggedV30_SupportsEightThreadsAndATimedSearch()
    {
        string path = Environment.GetEnvironmentVariable("STELLO_V30_LIBRARY")!;
        string hash = ReferenceLibraryFixture.Hash(path);
        MatchSide side = MatchCommands.ParseSide($"engine=stello-lib,lib={path},sha256={hash},time-ms=30,hash-bits=10,threads=8");
        using var reference = Assert.IsType<StelloLibraryMatchPlayer>(side.NewPlayer());
        reference.NewGame();

        EngineSearchResult result = reference.ChooseMove(EngineBoard.Initial, Player.Black, side.Limits, CancellationToken.None);

        Assert.NotNull(result.Move.Square);
        Assert.True(EngineBoard.Initial.IsLegal(Player.Black, result.Move.Square.Value));
        Assert.True(result.Nodes > 0);
    }

    [StelloV30Fact]
    public void TaggedV30_CancelsAnEightThreadSearchAndCanSearchAgain()
    {
        string path = Environment.GetEnvironmentVariable("STELLO_V30_LIBRARY")!;
        string hash = ReferenceLibraryFixture.Hash(path);
        MatchSide side = MatchCommands.ParseSide($"engine=stello-lib,lib={path},sha256={hash},depth=30,hash-bits=10,threads=8");
        using var reference = Assert.IsType<StelloLibraryMatchPlayer>(side.NewPlayer());
        reference.NewGame();
        SearchLimits limits = SearchLimits.FixedDepth(3, useOpeningBook: false) with { Threads = 8 };
        reference.ChooseMove(EngineBoard.Initial, Player.Black, limits, CancellationToken.None);
        reference.NewGame();
        using var cancellation = new CancellationTokenSource(TimeSpan.FromMilliseconds(100));

        Assert.Throws<OperationCanceledException>(() =>
            reference.ChooseMove(EngineBoard.Initial, Player.Black, side.Limits, cancellation.Token));

        reference.NewGame();
        EngineSearchResult result = reference.ChooseMove(EngineBoard.Initial, Player.Black, limits, CancellationToken.None);
        Assert.NotNull(result.Move.Square);
        Assert.True(EngineBoard.Initial.IsLegal(Player.Black, result.Move.Square.Value));
    }

    [StelloV30Fact]
    public async Task TaggedV30_TwelveSingleThreadWorkersCanSearchConcurrently() =>
        await AssertConcurrentWorkers(workers: 12, threads: 1);

    [StelloV30Fact]
    public async Task TaggedV30_TwoEightThreadWorkersCanSearchConcurrently() =>
        await AssertConcurrentWorkers(workers: 2, threads: 8);

    private static async Task AssertConcurrentWorkers(int workers, int threads)
    {
        string path = Environment.GetEnvironmentVariable("STELLO_V30_LIBRARY")!;
        string hash = ReferenceLibraryFixture.Hash(path);
        MatchSide side = MatchCommands.ParseSide($"engine=stello-lib,lib={path},sha256={hash},depth=6,hash-bits=10,threads={threads}");
        using var gate = new Barrier(workers);
        Task<EngineSearchResult>[] tasks = Enumerable.Range(0, workers).Select(_ => Task.Factory.StartNew(() =>
        {
            using var player = Assert.IsType<StelloLibraryMatchPlayer>(side.NewPlayer());
            player.NewGame();
            if (!gate.SignalAndWait(TimeSpan.FromSeconds(15)))
            {
                throw new TimeoutException("The reference-library workers did not all start.");
            }

            return player.ChooseMove(EngineBoard.Initial, Player.Black, side.Limits, CancellationToken.None);
        }, CancellationToken.None, TaskCreationOptions.LongRunning, TaskScheduler.Default)).ToArray();

        EngineSearchResult[] results = await Task.WhenAll(tasks);
        foreach (EngineSearchResult result in results)
        {
            Assert.NotNull(result.Move.Square);
            Assert.True(EngineBoard.Initial.IsLegal(Player.Black, result.Move.Square.Value));
            Assert.True(result.Nodes > 0);
            if (threads == 1)
            {
                AssertSearchEqual(results[0], result);
            }
        }
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static WeakReference SearchWithoutDisposing(string path, string hash)
    {
        var player = new StelloLibraryMatchPlayer(path, hash, hashBits: 10);
        player.NewGame();
        player.ChooseMove(EngineBoard.Initial, Player.Black, SearchLimits.FixedDepth(3, useOpeningBook: false), CancellationToken.None);
        return new WeakReference(player);
    }

    private string SideText(string settings) =>
        $"engine=stello-lib,lib={fixture.AliasPath},sha256={fixture.Sha256},{settings}";

    private static void AssertSearchEqual(EngineSearchResult expected, EngineSearchResult actual)
    {
        Assert.Equal(expected.Move, actual.Move);
        Assert.Equal(expected.Score, actual.Score);
        Assert.Equal(expected.Kind, actual.Kind);
        Assert.Equal(expected.Depth, actual.Depth);
        Assert.Equal(expected.Nodes, actual.Nodes);
        Assert.Equal(expected.Evaluations, actual.Evaluations);
    }

    private static void AssertMatchesBaseline(JsonElement expected, EngineSearchResult actual)
    {
        Assert.Equal(expected.GetProperty("Move").GetString(), actual.Move.ToString());
        Assert.Equal(expected.GetProperty("Score").GetInt32(), actual.Score);
        Assert.Equal(expected.GetProperty("Kind").GetString(), actual.Kind.ToString());
        Assert.Equal(expected.GetProperty("Depth").GetInt32(), actual.Depth);
        Assert.Equal(expected.GetProperty("Nodes").GetInt64(), actual.Nodes);
    }
}

public sealed class ReferenceLibraryFixture : IDisposable
{
    public ReferenceLibraryFixture()
    {
        string nativeFile = OperatingSystem.IsWindows()
            ? "stello_engine.dll"
            : OperatingSystem.IsMacOS() ? "libstello_engine.dylib" : "libstello_engine.so";
        OriginalPath = System.IO.Path.Combine(AppContext.BaseDirectory, nativeFile);
        Folder = Directory.CreateTempSubdirectory("Stello.ReferenceLibrary.").FullName;
        string withSpaces = Directory.CreateDirectory(System.IO.Path.Combine(Folder, "with spaces")).FullName;
        AliasPath = System.IO.Path.Combine(withSpaces, "stello_reference" + System.IO.Path.GetExtension(nativeFile));
        File.Copy(OriginalPath, AliasPath);
        Sha256 = Hash(AliasPath);
    }

    public string Folder { get; }

    public string OriginalPath { get; }

    public string AliasPath { get; }

    public string Sha256 { get; }

    public static string Hash(string path)
    {
        using FileStream stream = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(stream));
    }

    public void Dispose() => Directory.Delete(Folder, recursive: true);
}

public sealed class StelloV30FactAttribute : FactAttribute
{
    public StelloV30FactAttribute()
    {
        if (string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("STELLO_V30_LIBRARY")))
        {
            Skip = "Set STELLO_V30_LIBRARY to a separately built and renamed Stello 3.0 library.";
        }
    }
}
