using System.Diagnostics;
using Microsoft.JSInterop;
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
/// The searches use <see cref="Threads"/> threads: the worker then loads the threaded engine, if the page is
/// cross-origin isolated (else, and if it cannot be loaded, the single-threaded one, which searches with 1). Until the
/// Threads setting comes (Plans/making-stello-3-plan.md, phase 9), the number is 1 unless the page's address has a
/// <c>threads</c> query, for testing: <c>?threads=4</c>.
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
    private IJSInProcessObjectReference? _client;
    private Task _started;
    private Action<SearchInfo>? _progress;

    public WebEngineHost(IJSRuntime js, LocalStorageBookStore bookStore, OpeningBook book, int threads = 1)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(threads, 1);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(threads, SearchLimits.MaxThreads);
        Threads = threads;
        _js = js;
        _bookStore = bookStore;
        _book = book;
        _self = DotNetObjectReference.Create(this);
        _computer = new ComputerPlayer(new SearchEngine(), book, new Random());

        // Start at once, so the worker is usually ready before the computer's first search.
        _started = StartAsync();
    }

    public bool SupportsLearning => false;

    /// <summary>The threads of the searches (the worker caps them at the device's <c>navigator.hardwareConcurrency</c>).</summary>
    public int Threads { get; }

    /// <summary>The <c>threads</c> query of a page address (<c>?threads=4</c>), 1 to <see cref="SearchLimits.MaxThreads"/>, else 1.</summary>
    public static int ThreadsFromQuery(string uri)
    {
        string query = new Uri(uri).Query.TrimStart('?');
        foreach (string part in query.Split('&', StringSplitOptions.RemoveEmptyEntries))
        {
            string[] keyValue = part.Split('=', 2);
            if (keyValue.Length == 2 && keyValue[0] == "threads" && int.TryParse(keyValue[1], out int threads) && threads is >= 1 and <= SearchLimits.MaxThreads)
            {
                return threads;
            }
        }

        return 1;
    }

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
        Task<string> search = client.InvokeAsync<string>("call", "search", EngineProtocol.WriteRequest(board, player, limits with { Threads = Threads })).AsTask();
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
        await _client.InvokeAsync<string>("start", _self, HashBits, Threads);
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
