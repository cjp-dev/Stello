using System.IO;
using GameShell;
using Microsoft.JSInterop;
using Stello.App.Models;
using Stello.Engine;
using Stello.Web.Services;
using Stello.Web.Worker;

namespace Stello.Net.Tests;

public sealed class WebHostTests
{
    private static readonly ThreadCounts SingleThread = new(1, 1);

    [Fact]
    public void LocalStorageBookStore_Load_ReturnsNullOnJsSecurityException()
    {
        var js = new FakeJsRuntime { ThrowOnInvoke = new JSException("SecurityError: localStorage disabled") };
        var store = new LocalStorageBookStore(js);

        Assert.Null(store.Load());
    }

    [Fact]
    public void BrowserDialogService_Beep_SwallowsJsException()
    {
        var js = new FakeJsRuntime { ThrowOnInvoke = new JSException("AudioContext blocked") };
        var dialogs = new BrowserDialogService(js, new ShellDialogs(), SingleThread);

        Exception? ex = Record.Exception(() => dialogs.Beep());
        Assert.Null(ex);
    }

    [Fact]
    public async Task BrowserGameFileService_SaveAsync_WrapsJsExceptionInIoException()
    {
        var js = new FakeJsRuntime { ThrowOnInvokeAsync = new JSException("Download blocked") };
        var dialogs = new BrowserDialogService(js, new ShellDialogs(), SingleThread);
        var files = new BrowserGameFileService(js, dialogs);

        await Assert.ThrowsAsync<IOException>(() => files.SaveAsync("d3 c5", "game.stello", askForName: false));
    }

    [Fact]
    public void EngineProtocol_MoveNowResult_RejectsIllegalReportedMove()
    {
        var staleIllegal = new SearchInfo(6, Square.Parse("a1"), Square.Parse("h8"), 120, ScoreKind.Heuristic, 100, 90, TimeSpan.FromMilliseconds(5));
        SearchResult res = EngineProtocol.MoveNowResult(Board.Initial, Player.Black, staleIllegal, TimeSpan.FromMilliseconds(10));

        Assert.NotNull(res.Move.Square);
        Assert.True(Board.Initial.IsLegal(Player.Black, res.Move.Square.Value));
    }

    [Fact]
    public async Task WebEngineHost_IgnoresStaleProgressIds_AndCancelsImmediatelyDuringMoveNowGraceWindow()
    {
        var js = new FakeJsRuntime();
        var store = new LocalStorageBookStore(js);
        var book = OpeningBook.CreateEmpty();
        var host = new WebEngineHost(js, store, book, SingleThread, startThreads: 1);

        // First search: verifies stale progress IDs are dropped while active searchId progress is forwarded.
        var reports = new List<SearchInfo>();
        IProgress<SearchInfo> progress = new Progress<SearchInfo>(reports.Add);
        js.Client.OnSearch = (_, searchId) =>
        {
            host.OnProgress(searchId - 1, EngineProtocol.WriteProgress(new SearchInfo(1, Square.Parse("a1"), Square.Parse("a1"), -999, ScoreKind.Heuristic, 1, 1, TimeSpan.Zero)));
            host.OnProgress(searchId, EngineProtocol.WriteProgress(new SearchInfo(4, Square.Parse("d3"), Square.Parse("d3"), 50, ScoreKind.Heuristic, 10, 5, TimeSpan.FromMilliseconds(2))));
            return Task.FromResult(EngineProtocol.WriteResult(new SearchResult(new Move(Square.Parse("d3")), 50, ScoreKind.Heuristic, 4, 10, 5, TimeSpan.FromMilliseconds(2))));
        };

        SearchResult first = await host.ChooseMoveAsync(
            Board.Initial,
            Player.Black,
            SearchLimits.FixedDepth(4, useOpeningBook: false),
            progress,
            CancellationToken.None,
            CancellationToken.None);

        Assert.Equal(new Move(Square.Parse("d3")), first.Move);
        await Task.Delay(25);
        Assert.All(reports, r => Assert.Equal(4, r.Depth));

        // Second search: trigger MoveNow, then cancel immediately during 300ms grace window.
        var pendingSearch = new TaskCompletionSource<string>(TaskCreationOptions.RunContinuationsAsynchronously);
        js.Client.Signals.Clear();
        js.Client.OnSearch = (_, _) => pendingSearch.Task;

        using var cancel = new CancellationTokenSource();
        using var moveNow = new CancellationTokenSource();
        Task<SearchResult> searchTask = host.ChooseMoveAsync(
            Board.Initial,
            Player.Black,
            SearchLimits.FixedDepth(10, useOpeningBook: false),
            null,
            cancel.Token,
            moveNow.Token);

        await Task.Delay(20);
        moveNow.Cancel();
        await Task.Delay(15);
        cancel.Cancel();

        await Assert.ThrowsAnyAsync<OperationCanceledException>(async () => await searchTask.WaitAsync(TimeSpan.FromMilliseconds(200)));
        Assert.Equal([1, 2], js.Client.Signals);
    }

    private sealed class FakeJsRuntime : IJSInProcessRuntime
    {
        public FakeClient Client { get; } = new();
        public JSException? ThrowOnInvoke { get; init; }
        public JSException? ThrowOnInvokeAsync { get; init; }

        public TResult Invoke<TResult>(string identifier, params object?[]? args)
        {
            if (ThrowOnInvoke is not null)
            {
                throw ThrowOnInvoke;
            }
            return default!;
        }

        public ValueTask<TValue> InvokeAsync<TValue>(string identifier, object?[]? args) =>
            InvokeAsync<TValue>(identifier, CancellationToken.None, args);

        public ValueTask<TValue> InvokeAsync<TValue>(string identifier, CancellationToken cancellationToken, object?[]? args)
        {
            if (ThrowOnInvokeAsync is not null)
            {
                throw ThrowOnInvokeAsync;
            }
            if (identifier == "import" && typeof(TValue) == typeof(IJSInProcessObjectReference))
            {
                return ValueTask.FromResult((TValue)(object)Client);
            }
            return ValueTask.FromResult(default(TValue)!);
        }
    }

    private sealed class FakeClient : IJSInProcessObjectReference
    {
        public List<int> Signals { get; } = [];
        public Func<string, long, Task<string>>? OnSearch { get; set; }

        public TValue Invoke<TValue>(string identifier, params object?[]? args)
        {
            if (identifier == "signal" && args is [int code] && typeof(TValue) == typeof(bool))
            {
                Signals.Add(code);
                return (TValue)(object)true;
            }
            return default!;
        }

        public ValueTask<TValue> InvokeAsync<TValue>(string identifier, object?[]? args) =>
            InvokeAsync<TValue>(identifier, CancellationToken.None, args);

        public async ValueTask<TValue> InvokeAsync<TValue>(string identifier, CancellationToken cancellationToken, object?[]? args)
        {
            if (identifier == "start" && typeof(TValue) == typeof(string))
            {
                return (TValue)(object)"5.0";
            }
            if (identifier == "call" && args is ["search", string reqJson, long searchId] && typeof(TValue) == typeof(string))
            {
                string res = OnSearch is not null
                    ? await OnSearch(reqJson, searchId)
                    : EngineProtocol.WriteResult(new SearchResult(new Move(Square.Parse("d3")), 0, ScoreKind.None, 1, 1, 1, TimeSpan.Zero));
                return (TValue)(object)res;
            }
            return default!;
        }

        public void Dispose() { }
        public ValueTask DisposeAsync() => ValueTask.CompletedTask;
    }
}
