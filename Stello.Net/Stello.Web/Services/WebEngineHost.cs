using System.Diagnostics;
using Microsoft.JSInterop;
using Stello.App.Services;
using Stello.Engine;
using Stello.Web.Worker;

namespace Stello.Web.Services;

/// <summary>
/// Runs the engine in a Web Worker so the page stays responsive. A running search cannot be interrupted inside
/// the worker, so Stop and Move Now terminate it and start a new one (with a fresh hash table and book tracker).
/// </summary>
public sealed class WebEngineHost : IEngineHost
{
    private const int HashBits = 19;

    private readonly IJSRuntime _js;
    private readonly LocalStorageBookStore _bookStore;
    private readonly DotNetObjectReference<WebEngineHost> _self;
    private byte[] _book;
    private IJSInProcessObjectReference? _client;
    private Task _started;
    private Action<SearchInfo>? _progress;

    public WebEngineHost(IJSRuntime js, LocalStorageBookStore bookStore, OpeningBook book)
    {
        _js = js;
        _bookStore = bookStore;
        _self = DotNetObjectReference.Create(this);
        using var stream = new MemoryStream();
        book.Save(stream);
        _book = stream.ToArray();

        // Start at once, so the worker is usually ready before the computer's first move.
        _started = StartAsync();
    }

    public bool SupportsLearning => false;

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
        Task<string> search = client.InvokeAsync<string>("call", "chooseMove", EngineProtocol.WriteRequest(board, player, limits)).AsTask();
        try
        {
            if (await Task.WhenAny(search, stopped.Task) == search)
            {
                return EngineProtocol.ReadResult(await search);
            }
        }
        finally
        {
            _progress = null;
        }

        Forget(search);
        Restart(client);
        if (!await stopped.Task)
        {
            throw new OperationCanceledException(cancellationToken);
        }

        return EngineProtocol.MoveNowResult(board, player, last, clock.Elapsed);
    }

    public void ResetBookTracker()
    {
        // A worker that is still starting has a fresh tracker anyway.
        if (_client is not null && _started.IsCompletedSuccessfully)
        {
            Forget(_client.InvokeVoidAsync("call", "resetBookTracker").AsTask());
        }
    }

    public async Task<BookUpdate> AddGameToBookAsync(IReadOnlyList<Move> moves, GameResult result)
    {
        IJSInProcessObjectReference client = await StartedAsync();
        int nodeCount = await client.InvokeAsync<int>("call", "addGame", string.Join(' ', moves), (int)result);
        _book = await client.InvokeAsync<byte[]>("call", "getBook");
        return new BookUpdate(nodeCount, _bookStore.Save(_book));
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
        await _client.InvokeVoidAsync("start", _self, _book, HashBits);
    }

    private void Restart(IJSInProcessObjectReference client)
    {
        client.InvokeVoid("terminate");
        _started = StartAsync();
        Forget(_started);
    }
}
