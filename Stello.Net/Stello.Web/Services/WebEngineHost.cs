using System.Diagnostics;
using Microsoft.JSInterop;
using Stello.App.Models;
using Stello.App.Services;
using Stello.Engine;
using Stello.Web.Worker;

namespace Stello.Web.Services;

/// <summary>
/// Runs the engine in a Web Worker so the page stays responsive: Stello's Rust engine compiled to WebAssembly
/// (wwwroot/js/engine-worker.js). The opening book stays on the page: the book move and the added games are handled
/// here, and the worker only searches. A running search is stopped cooperatively via a SharedArrayBuffer signal
/// (preserving the hash tables), falling back to restarting the worker if needed.
/// </summary>
/// <remarks>
/// A search with more than 1 thread (the Threads setting, <see cref="SearchLimits.Threads"/>) needs the threaded engine,
/// which the worker loads at its start if the page is cross-origin isolated (else, or if it cannot be loaded, the
/// single-threaded one, which searches with 1). The worker chooses its engine only when it starts, so a search with
/// more than 1 thread after one with 1, or the other way round, restarts it; another number of threads above 1 does
/// not.
/// </remarks>
public sealed class WebEngineHost : IEngineHost
{
    private const int HashBits = 19;

    private readonly IJSRuntime _js;
    private readonly LocalStorageBookStore _bookStore;
    private readonly OpeningBook _book;
    private readonly DotNetObjectReference<WebEngineHost> _self;

    // Only its book step runs here (TryChooseBookMove); its engine never searches, so it needs no native library.
    private readonly ComputerPlayer _computer;
    private int _workerHashBits = HashBits;
    // The threads the worker was started for: its engine is the threaded one if this is more than 1.
    private int _workerThreads;
    private IJSInProcessObjectReference? _client;
    private Task _started;
    private Action<SearchInfo>? _progress;

    /// <param name="threads">The browser's thread counts (navigator.hardwareConcurrency).</param>
    /// <param name="startThreads">The threads of the first searches (the saved setting), for the worker's first engine.</param>
    public WebEngineHost(IJSRuntime js, LocalStorageBookStore bookStore, OpeningBook book, ThreadCounts threads, int startThreads = 1)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(startThreads, 1);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(startThreads, SearchLimits.MaxThreads);
        Threads = threads;
        _workerThreads = startThreads;
        _js = js;
        _bookStore = bookStore;
        _book = book;
        _self = DotNetObjectReference.Create(this);
        _computer = new ComputerPlayer(new SearchEngine(), book, new Random());

        // Start at once, so the worker is usually ready before the computer's first search.
        _started = StartAsync();
    }

    public bool SupportsLearning => false;

    public ThreadCounts Threads { get; }

    public async Task<SearchResult> ChooseMoveAsync(
        Board board,
        Player player,
        SearchLimits limits,
        IProgress<SearchInfo>? progress,
        CancellationToken cancellationToken,
        CancellationToken moveNowToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        var clock = Stopwatch.StartNew();

        // The worker of Stello 2.0 made a new ComputerPlayer, with a new book tracker, for other hash bits.
        if (limits.HashBits != _workerHashBits)
        {
            _workerHashBits = limits.HashBits;
            _computer.BookTracker.Reset();
        }

        if (_computer.TryChooseBookMove(board, player, limits, out SearchResult? bookMove))
        {
            return bookMove;
        }

        // The worker's engine (threaded or not) is chosen when it starts: a new worker for the other one. Its hash tables
        // are new too; the book tracker stays (unlike after a failed search).
        if ((limits.Threads > 1) != (_workerThreads > 1))
        {
            _workerThreads = limits.Threads;
            (await StartedAsync()).InvokeVoid("terminate");
            _started = StartAsync();
        }

        IJSInProcessObjectReference client = await StartedAsync();

        SearchInfo? last = null;
        var stopped = new TaskCompletionSource<bool>();
        using CancellationTokenRegistration cancelled = cancellationToken.Register(() => stopped.TrySetResult(false));
        using CancellationTokenRegistration movedNow = moveNowToken.Register(() => stopped.TrySetResult(true));

        _progress = info =>
        {
            last = info;
            progress?.Report(info);
        };
        Task<string> search = client.InvokeAsync<string>("call", "search", EngineProtocol.WriteRequest(board, player, limits)).AsTask();
        try
        {
            if (await Task.WhenAny(search, stopped.Task) == search)
            {
                return EngineProtocol.ReadResult(await Searched(search, client));
            }

            bool isMoveNow = await stopped.Task;
            bool signaled = client.Invoke<bool>("signal", isMoveNow ? 1 : 2);
            if (signaled)
            {
                Task completed = await Task.WhenAny(search, Task.Delay(300));
                if (completed == search)
                {
                    if (!isMoveNow)
                    {
                        Forget(search);
                        throw new OperationCanceledException(cancellationToken);
                    }
                    try
                    {
                        return EngineProtocol.ReadResult(await search);
                    }
                    catch (JSException)
                    {
                        // Fallback if worker threw
                    }
                }
            }

            Forget(search);
            Restart(client);
            if (!isMoveNow)
            {
                throw new OperationCanceledException(cancellationToken);
            }

            return EngineProtocol.MoveNowResult(board, player, last, clock.Elapsed);
        }
        finally
        {
            _progress = null;
        }
    }

    public void ResetBookTracker() => _computer.BookTracker.Reset();

    public Task<BookUpdate> AddGameToBookAsync(IReadOnlyList<Move> moves, GameResult result)
    {
        // Adding a game does not search, so the limits do not matter, and the engine never loads its native library.
        new BookLearner(_book, new SearchEngine(hashBits: 10), SearchLimits.FixedDepth(1), new Random()).AddGame(moves, result);
        return Task.FromResult(new BookUpdate(_book.NodeCount, _bookStore.Save(_book)));
    }

    public Task<BookLearningSummary> LearnAsync(
        BookLearningKind kind,
        SearchLimits limits,
        IProgress<BookLearningProgress>? progress,
        CancellationToken cancellationToken) =>
        throw new NotSupportedException("Book learning is only available in the desktop version.");

    [JSInvokable]
    public void OnProgress(string json) => _progress?.Invoke(EngineProtocol.ReadProgress(json));

    private static void Forget(Task task) =>
        task.ContinueWith(static t => _ = t.Exception, TaskContinuationOptions.OnlyOnFaulted);

    // A search that fails (the WebAssembly engine trapped) may leave the engine unusable, so the next search gets a
    // new worker.
    private async Task<string> Searched(Task<string> search, IJSInProcessObjectReference client)
    {
        try
        {
            return await search;
        }
        catch (JSException)
        {
            Restart(client);
            throw;
        }
    }

    private async Task<IJSInProcessObjectReference> StartedAsync()
    {
        if (_started.IsFaulted)
        {
            _started = StartAsync();
        }

        await _started;
        return _client!;
    }

    private async Task StartAsync()
    {
        _client ??= await _js.InvokeAsync<IJSInProcessObjectReference>("import", "./js/engine-client.js");
        await _client.InvokeAsync<string>("start", _self, HashBits, _workerThreads);
    }

    private void Restart(IJSInProcessObjectReference client)
    {
        client.InvokeVoid("terminate");
        _started = StartAsync();
        Forget(_started);

        // The worker of Stello 2.0 kept the book tracker, so a new worker started with a new one (for HashBits).
        _workerHashBits = HashBits;
        _computer.BookTracker.Reset();
    }
}
