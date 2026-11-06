using System.Runtime.CompilerServices;
using Stello.Engine.Benchmark;
using Stello.Engine.Evaluation;
using Stello.Engine.Native;
using Stello.Engine.Search;
using V2 = Stello.Engine.V2;

namespace Stello.Engine.Tests;

/// <summary>
/// The <see cref="SearchEngine"/> wrapper over the Rust engine (stello_engine): the ABI, the options, the callbacks and
/// the stop signals. The frozen C# engine is used as a correctness reference for moves and scores.
/// </summary>
public class NativeEngineTests
{
    private static readonly Board Midgame = TestGames.Play("f5 d6 c3 d3 c4 f4 c5 b3 c2 e6").Board;

    [Fact]
    public void Library_HasTheVersionAndLayoutOfThisWrapper()
    {
        Assert.Equal(30000u, StelloNative.Version());
        Assert.Equal(3u, StelloNative.AbiVersion);
        Assert.Equal(StelloNative.AbiVersion, StelloNative.LibraryAbiVersion());
        Assert.Equal(48, Unsafe.SizeOf<NativeSearchInfo>());
        Assert.Equal(40, Unsafe.SizeOf<NativeSearchResult>());
    }

    public static TheoryData<string> OptionCases() => ["selective off", "probcut 150", "soft 50", "written model", "loaded model"];

    [Theory]
    [MemberData(nameof(OptionCases))]
    public void Options_SearchLikeStelloTwo(string option)
    {
        (SearchOptions options, V2.Search.SearchOptions v2Options) = option switch
        {
            "selective off" => (SearchOptions.Default with { Selective = false }, V2.Search.SearchOptions.Default with { Selective = false }),
            "probcut 150" => (SearchOptions.Default with { ProbCutPercent = 150 }, V2.Search.SearchOptions.Default with { ProbCutPercent = 150 }),
            "soft 50" => (SearchOptions.Default with { SoftTimePercent = 50 }, V2.Search.SearchOptions.Default with { SoftTimePercent = 50 }),
            "written model" => WrittenModel(),
            _ => LoadedModel(),
        };
        var engine = new SearchEngine(16, options);
        var v2 = new V2.SearchEngine(16, v2Options);

        foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 31, count: 12, plies: 24)
            .Concat(TestPositions.RandomPositions(seed: 32, count: 3, plies: 46)))
        {
            SearchLimits limits = board.EmptyCount <= 18 ? SearchLimits.Solve : SearchLimits.FixedDepth(6);
            SearchResult expected = v2.Search(board, player, limits);
            SearchResult actual = engine.Search(board, player, limits);
            Assert.Equal((expected.Move, expected.Score, expected.Kind, expected.Depth), (actual.Move, actual.Score, actual.Kind, actual.Depth));
        }
    }

    [Fact]
    public void UpperBoundEqualityCutoff_IsUsedByDefault()
    {
        BenchmarkPosition position = BenchmarkPosition.Load(
            Path.Combine(AppContext.BaseDirectory, "Data", "Benchmark", "midgame.txt")).Single(p => p.Id == "g01e48");
        SearchLimits limits = SearchLimits.FixedDepth(10);
        SearchResult result = new SearchEngine().Search(position.Board, position.Player, limits);
        Assert.Equal(37459, result.Nodes);
    }

    [Fact]
    public void ProgressAndIterations_PreserveMovesAndScores()
    {
        var engine = new SearchEngine(16);
        var v2 = new V2.SearchEngine(16);
        foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 33, count: 5, plies: 20))
        {
            var reports = new List<string>();
            var iterations = new List<(int, int)>();
            engine.IterationDone = (depth, score) => iterations.Add((depth, score));
            engine.Search(board, player, SearchLimits.FixedDepth(7), new SyncProgress<SearchInfo>(info => reports.Add(Report(info))));

            var v2Reports = new List<string>();
            var v2Iterations = new List<(int, int)>();
            v2.IterationDone = (depth, score) => v2Iterations.Add((depth, score));
            v2.Search(board, player, SearchLimits.FixedDepth(7), new SyncProgress<SearchInfo>(info => v2Reports.Add(Report(info))));

            // Fast searches have no heartbeats; node and evaluation counts may differ after search fixes.
            Assert.Equal(v2Reports, reports);
            Assert.Equal(v2Iterations, iterations);
            Assert.Equal(7, iterations.Count);
        }
    }

    [Fact]
    public void ExceptionInProgress_IsThrownBySearch_AndTheEngineStillWorks()
    {
        var engine = new SearchEngine(16);
        int reports = 0;
        var progress = new SyncProgress<SearchInfo>(_ =>
        {
            if (++reports == 3)
            {
                throw new InvalidTimeZoneException("from the progress");
            }
        });

        var exception = Assert.Throws<InvalidTimeZoneException>(() => engine.Search(Midgame, Player.Black, SearchLimits.FixedDepth(60), progress));

        Assert.Equal("from the progress", exception.Message);
        Assert.Equal(3, reports);
        Assert.Equal(ScoreKind.Heuristic, engine.Search(Midgame, Player.Black, SearchLimits.FixedDepth(4)).Kind);
    }

    [Fact]
    public void ExceptionInIterationDone_IsThrownBySearch()
    {
        var engine = new SearchEngine(16) { IterationDone = (depth, _) => throw new InvalidTimeZoneException($"depth {depth}") };

        var exception = Assert.Throws<InvalidTimeZoneException>(() => engine.Search(Midgame, Player.Black, SearchLimits.FixedDepth(60)));

        Assert.Equal("depth 1", exception.Message);
        engine.IterationDone = null;
        Assert.Equal(4, engine.Search(Midgame, Player.Black, SearchLimits.FixedDepth(4)).Depth);
    }

    [Fact]
    public void CancelAndMoveNow_FromInsideTheProgress()
    {
        // As the web app's worker does: the progress callback sets the tokens.
        var engine = new SearchEngine(16);
        using var cancel = new CancellationTokenSource();
        int reports = 0;
        Assert.Throws<OperationCanceledException>(() => engine.Search(Midgame, Player.Black, SearchLimits.FixedDepth(60),
            new SyncProgress<SearchInfo>(_ =>
            {
                if (++reports == 50)
                {
                    cancel.Cancel();
                }
            }), cancel.Token));

        using var moveNow = new CancellationTokenSource();
        reports = 0;
        SearchResult result = engine.Search(Midgame, Player.Black, SearchLimits.FixedDepth(60),
            new SyncProgress<SearchInfo>(_ =>
            {
                if (++reports == 50)
                {
                    moveNow.Cancel();
                }
            }), moveNowToken: moveNow.Token);
        Assert.Equal(ScoreKind.Heuristic, result.Kind);
        Assert.True(Midgame.IsLegal(Player.Black, result.Move.Square!.Value));
    }

    [Fact]
    public void Tokens_FromAnEarlierSearch_DoNotStopTheNextOne()
    {
        var engine = new SearchEngine(16);
        using var moveNow = new CancellationTokenSource();
        moveNow.Cancel();
        // Stopped at the first abort check, as Stello 2.0 stops.
        SearchResult expected = new V2.SearchEngine(16).Search(Midgame, Player.Black, SearchLimits.FixedDepth(60), moveNowToken: moveNow.Token);
        SearchResult actual = engine.Search(Midgame, Player.Black, SearchLimits.FixedDepth(60), moveNowToken: moveNow.Token);
        Assert.Equal((expected.Move, expected.Score, expected.Kind, expected.Depth), (actual.Move, actual.Score, actual.Kind, actual.Depth));

        Assert.Equal(9, engine.Search(Midgame, Player.Black, SearchLimits.FixedDepth(9)).Depth);
    }

    [Fact]
    public void EnginesOnSeveralThreads_SearchAsOnOne()
    {
        (Board, Player)[] positions = TestPositions.RandomPositions(seed: 34, count: 8, plies: 22).ToArray();
        string[] Run(SearchEngine engine) => positions.Select(p =>
        {
            engine.ClearHash();
            return Signature(engine.Search(p.Item1, p.Item2, SearchLimits.FixedDepth(7)));
        }).ToArray();
        string[] expected = Run(new SearchEngine(14));

        string[][] parallel = Enumerable.Range(0, 4).AsParallel().WithDegreeOfParallelism(4)
            .Select(_ => Run(new SearchEngine(14)))
            .ToArray();

        Assert.All(parallel, run => Assert.Equal(expected, run));
    }

    [Fact]
    public void Threads_AreOneByDefaultAndChecked()
    {
        Assert.Equal(1, SearchLimits.FixedDepth(5).Threads);
        Assert.Equal(1, SearchLimits.Solve.Threads);
        Assert.Equal(8, (SearchLimits.Solve with { Threads = 8 }).Threads);
        Assert.Throws<ArgumentOutOfRangeException>(() => SearchLimits.Solve with { Threads = 0 });
        Assert.Throws<ArgumentOutOfRangeException>(() => SearchLimits.Solve with { Threads = SearchLimits.MaxThreads + 1 });
    }

    [Fact]
    public void SeveralThreads_SolveExactly_AndOneThreadIsUnchangedAfterwards()
    {
        // The same engine switches between 4 threads and 1; with 1 it must recover its original result.
        var engine = new SearchEngine(16);
        var v2 = new V2.SearchEngine(16);
        foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 36, count: 6, plies: 42))
        {
            engine.ClearHash();
            v2.ClearHash();
            SearchResult expected = v2.Search(board, player, SearchLimits.Solve);
            SearchResult singleThreaded = engine.Search(board, player, SearchLimits.Solve);
            engine.ClearHash();
            SearchResult threaded = engine.Search(board, player, SearchLimits.Solve with { Threads = 4 });
            Assert.Equal((expected.Score, expected.Kind), (threaded.Score, threaded.Kind));

            SearchResult midgame = engine.Search(board, player, SearchLimits.FixedDepth(8) with { Threads = 3 });
            Assert.True(midgame.Move.IsPass ? !board.HasLegalMove(player) : board.IsLegal(player, midgame.Move.Square!.Value));

            engine.ClearHash();
            Assert.Equal(Signature(singleThreaded), Signature(engine.Search(board, player, SearchLimits.Solve)));
        }
    }

    [Fact]
    public void LazySmpAlone_SolvesExactly()
    {
        var engine = new SearchEngine(16, SearchOptions.Default with { Ybwc = false });
        var v2 = new V2.SearchEngine(16);
        foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 37, count: 3, plies: 40))
        {
            SearchResult expected = v2.Search(board, player, SearchLimits.Solve);
            SearchResult threaded = engine.Search(board, player, SearchLimits.Solve with { Threads = 4 });
            Assert.Equal((expected.Score, expected.Kind), (threaded.Score, threaded.Kind));
        }
    }

    [Fact]
    public void StelloTwo_IgnoresThreads()
    {
        var v2 = new V2.SearchEngine(16);
        SearchResult one = v2.Search(Midgame, Player.Black, SearchLimits.FixedDepth(7));
        v2.ClearHash();
        Assert.Equal(Signature(one), Signature(v2.Search(Midgame, Player.Black, SearchLimits.FixedDepth(7) with { Threads = 4 })));
    }

    [Fact]
    public void Constructor_RejectsOptionsTheRustEngineDoesNotTake()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => new SearchEngine(9));
        Assert.Throws<ArgumentOutOfRangeException>(() => new SearchEngine(27));
        Assert.Throws<ArgumentOutOfRangeException>(() => new SearchEngine(16, SearchOptions.Default with { SoftTimePercent = 101 }));
        Assert.Throws<ArgumentOutOfRangeException>(() => new SearchEngine(16, SearchOptions.Default with { ProbCutPercent = -1 }));
    }

    [Fact]
    public void ClearHash_BeforeTheFirstSearch_DoesNothing()
    {
        var engine = new SearchEngine(10);
        engine.ClearHash();
        Assert.Equal(10, engine.HashBits);
    }

    private static (SearchOptions, V2.Search.SearchOptions) WrittenModel()
    {
        // A classic11 model with random weights, which the wrapper passes to Rust as EvalWeights.Write writes it.
        PatternSet set = PatternSet.Classic11;
        var random = new Random(35);
        var tables = new short[4][];
        for (int stage = 0; stage < tables.Length; stage++)
        {
            tables[stage] = new short[set.RawSize];
            for (int s = 0; s < set.Shapes.Count; s++)
            {
                PatternShape shape = set.Shapes[s];
                var folded = new short[shape.Size];
                foreach (int representative in shape.Representatives)
                {
                    folded[representative] = (short)random.Next(-300, 300);
                }

                for (int raw = 0; raw < shape.Size; raw++)
                {
                    tables[stage][set.Offsets[s] + raw] = folded[shape.Fold[raw]];
                }
            }
        }

        var model = new EvalWeights(set, EvalWeights.EvenStages(tables.Length), 50, tables);
        using var stream = new MemoryStream();
        model.Write(stream);
        stream.Position = 0;
        return (new SearchOptions { Pattern = model }, new V2.Search.SearchOptions { Pattern = V2.Evaluation.EvalWeights.Read(stream) });
    }

    private static (SearchOptions, V2.Search.SearchOptions) LoadedModel()
    {
        // The shipped file loaded as another model: the wrapper passes the file's bytes.
        string path = Path.Combine(AppContext.BaseDirectory, "Data", "stello-eval.bin");
        return (new SearchOptions { Pattern = EvalWeights.Load(path) }, new V2.Search.SearchOptions { Pattern = V2.Evaluation.EvalWeights.Load(path) });
    }

    private static string Signature(SearchResult result) =>
        $"{result.Move} {result.Score} {result.Kind} {result.Depth} {result.Nodes} {result.Evaluations}";

    private static string Report(SearchInfo info) =>
        $"{info.Depth} {info.CurrentMove} {info.BestMove} {info.Score} {info.Kind}";
}
