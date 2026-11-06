using System.Runtime.CompilerServices;
using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;
using Stello.Engine.Native;
using Stello.Engine.Search;

namespace Stello.Engine;

/// <summary>
/// Stello's search: the midgame search ported from Minmax.cpp and Kontrol.cpp (iterative deepening with a hash
/// table, response killers and selective search), with the trained pattern evaluation, and the endgame solver
/// adapted from Egaroucid. Since Stello 3.0 the search runs in Rust (<c>Stello.Rust/stello-core</c>), ported from the
/// C# engine of Stello 2.0 kept in <c>Stello.Engine.V2</c>; this class calls it through the C ABI of
/// <c>stello_engine</c> (<see cref="StelloNative"/>). An instance runs one search at a time; instances on different
/// threads are independent. A search with <see cref="SearchLimits.Threads"/> above 1 also runs on worker threads of the
/// Rust engine, which share its hash tables; they are started at the first such search and kept for the next ones.
/// </summary>
/// <remarks>
/// The Rust engine and its hash tables are made at the first search, so an engine that never searches (such as the
/// one a <see cref="BookLearner"/> only adds games with) needs no native library.
/// </remarks>
public sealed class SearchEngine
{
    /// <summary>A finished game in the midgame search scores this plus the disc difference (C++: the same).</summary>
    internal const int WinScore = 32600;

    /// <summary>Multi-ProbCut's t × 100 when <see cref="SearchOptions.ProbCutPercent"/> is null.</summary>
    internal const int DefaultProbCutPercent = 100;

    // Two hash tables of 2^hashBits slots of 64 bytes (two 24-byte entries and the slot's lock, one cache line).
    private const int HashBytesPerSlot = 2 * 64;

    private EngineHandle? _engine;
    private SignalsHandle? _signals;

    // The threads the Rust engine was last set to (a new one uses 1).
    private int _threads = 1;

    /// <param name="hashBits">Each of the two hash tables gets 2^hashBits slots of two entries (C++: HASHSIZE 19, one entry).</param>
    public SearchEngine(int hashBits = 19)
        : this(hashBits, SearchOptions.Default)
    {
    }

    internal SearchEngine(int hashBits, SearchOptions options)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(hashBits, 10);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(hashBits, 26);
        ArgumentNullException.ThrowIfNull(options);
        if (options.SoftTimePercent is < 0 or > 100)
        {
            throw new ArgumentOutOfRangeException(nameof(options), options.SoftTimePercent, "SoftTimePercent must be 0-100.");
        }

        if (options.ProbCutPercent is < 0 or > 100_000)
        {
            throw new ArgumentOutOfRangeException(nameof(options), options.ProbCutPercent, "ProbCutPercent must be 0-100000.");
        }

        HashBits = hashBits;
        Options = options;
    }

    public int HashBits { get; }

    internal SearchOptions Options { get; }

    /// <summary>Called after each finished midgame iteration with its depth and score (for the Multi-ProbCut fit).</summary>
    internal Action<int, int>? IterationDone { get; set; }

    public void ClearHash()
    {
        // Before the first search the tables are empty.
        if (_engine is not null)
        {
            Check(StelloNative.ClearHash(_engine));
        }
    }

    /// <summary>Finds the best move for <paramref name="player"/> (C++: getcomputer).</summary>
    /// <param name="cancellationToken">Stops the search and throws <see cref="OperationCanceledException"/>.</param>
    /// <param name="moveNowToken">Stops the search and returns the best move found so far.</param>
    /// <param name="onlyMoves">
    /// Search only these moves, even if just one is left (C++ calclib: the best move not yet in the book).
    /// </param>
    /// <exception cref="DllNotFoundException">The Rust engine library is missing.</exception>
    public unsafe SearchResult Search(
        Board board,
        Player player,
        SearchLimits limits,
        IProgress<SearchInfo>? progress = null,
        CancellationToken cancellationToken = default,
        CancellationToken moveNowToken = default,
        ulong? onlyMoves = null)
    {
        ArgumentNullException.ThrowIfNull(limits);
        cancellationToken.ThrowIfCancellationRequested();
        (EngineHandle engine, SignalsHandle signals) = Native();

        var state = new Callbacks(progress, IterationDone, signals);
        GCHandle handle = GCHandle.Alloc(state);
        int status;
        NativeSearchResult result;
        try
        {
            nint user = GCHandle.ToIntPtr(handle);
            StelloNative.ResetSignals(signals);
            if (limits.Threads != _threads)
            {
                Check(StelloNative.SetThreads(engine, limits.Threads));
                _threads = limits.Threads;
            }

            if (state.IterationDone is not null)
            {
                Check(StelloNative.SetIterationDone(engine, &OnIterationDone, user));
            }

            try
            {
                using CancellationTokenRegistration cancel = cancellationToken.UnsafeRegister(static s => StelloNative.Cancel((SignalsHandle)s!), signals);
                using CancellationTokenRegistration moveNow = moveNowToken.UnsafeRegister(static s => StelloNative.MoveNow((SignalsHandle)s!), signals);
                status = StelloNative.Search(
                    engine,
                    board.Black,
                    board.White,
                    (int)player,
                    (int)limits.Mode,
                    limits.Depth,
                    (long)limits.Time.TotalMilliseconds,
                    onlyMoves ?? 0,
                    onlyMoves is null ? 0 : 1,
                    signals,
                    progress is null ? null : &OnProgress,
                    user,
                    out result);
            }
            finally
            {
                if (state.IterationDone is not null)
                {
                    StelloNative.SetIterationDone(engine, null, 0);
                }
            }
        }
        finally
        {
            handle.Free();
        }

        // A callback that threw stopped the search; its exception is the search's.
        state.Failure?.Throw();
        switch (status)
        {
            case StelloNative.Cancelled:
                throw new OperationCanceledException(cancellationToken);
            case StelloNative.NoLegalOnlyMove:
                throw new ArgumentException("None of the moves to search is legal.", nameof(onlyMoves));
            default:
                Check(status);
                break;
        }

        return new SearchResult(
            result.Move >= 0 ? new Move(new Square(result.Move)) : Move.Pass,
            result.Score,
            (ScoreKind)result.Kind,
            result.Depth,
            (long)result.Nodes,
            (long)result.Evaluations,
            StelloNative.Elapsed(result.ElapsedNs));
    }

    // The Rust engine and the stop signals, made at the first search.
    private unsafe (EngineHandle Engine, SignalsHandle Signals) Native()
    {
        if (_engine is not null && _signals is not null)
        {
            return (_engine, _signals);
        }

        StelloNative.EnsureLoaded();
        byte[]? eval = Options.Pattern?.FileBytes;
        EngineHandle engine;
        int status;
        fixed (byte* bytes = eval)
        {
            engine = StelloNative.CreateEngine(
                HashBits,
                Options.Selective ? 1 : 0,
                Options.ProbCutPercent ?? -1,
                Options.SoftTimePercent ?? -1,
                bytes,
                (nuint)(eval?.Length ?? 0),
                out status);
        }

        if (engine.IsInvalid)
        {
            engine.Dispose();
            throw status == StelloNative.InvalidEvaluation
                ? new InvalidDataException("The Rust engine could not read the evaluation (SearchOptions.Pattern).")
                : new InvalidOperationException($"The Rust engine could not be made (status {status}).");
        }

        if (!Options.Ybwc)
        {
            Check(StelloNative.SetYbwc(engine, 0));
        }

        engine.AddMemoryPressure((long)HashBytesPerSlot << HashBits);
        _signals = StelloNative.CreateSignals();
        _engine = engine;
        return (_engine, _signals);
    }

    private static void Check(int status)
    {
        switch (status)
        {
            case StelloNative.Ok:
                return;
            case StelloNative.InvalidArgument:
                throw new ArgumentException("The Rust engine rejected the arguments of the search.");
            default:
                throw new InvalidOperationException($"The Rust engine failed (status {status}).");
        }
    }

    // Called by Rust on the searching thread, during the search. An exception must not reach Rust: the first one is
    // kept, the search is cancelled, and Search throws it.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static unsafe void OnProgress(NativeSearchInfo* info, nint user)
    {
        var state = (Callbacks)GCHandle.FromIntPtr(user).Target!;
        if (state.Failure is not null || state.Progress is null)
        {
            return;
        }

        try
        {
            state.Progress.Report(new SearchInfo(
                info->Depth,
                new Square(info->CurrentMove),
                info->BestMove >= 0 ? new Square(info->BestMove) : null,
                info->Score,
                (ScoreKind)info->Kind,
                (long)info->Nodes,
                (long)info->Evaluations,
                StelloNative.Elapsed(info->ElapsedNs)));
        }
        catch (Exception exception)
        {
            state.Fail(exception);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void OnIterationDone(int depth, int score, nint user)
    {
        var state = (Callbacks)GCHandle.FromIntPtr(user).Target!;
        if (state.Failure is not null || state.IterationDone is null)
        {
            return;
        }

        try
        {
            state.IterationDone(depth, score);
        }
        catch (Exception exception)
        {
            state.Fail(exception);
        }
    }

    /// <summary>What the callbacks of one search need.</summary>
    private sealed class Callbacks(IProgress<SearchInfo>? progress, Action<int, int>? iterationDone, SignalsHandle signals)
    {
        public IProgress<SearchInfo>? Progress { get; } = progress;

        public Action<int, int>? IterationDone { get; } = iterationDone;

        public ExceptionDispatchInfo? Failure { get; private set; }

        public void Fail(Exception exception)
        {
            Failure = ExceptionDispatchInfo.Capture(exception);
            StelloNative.Cancel(signals);
        }
    }
}
