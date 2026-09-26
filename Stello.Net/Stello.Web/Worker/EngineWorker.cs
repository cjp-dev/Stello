using System.Diagnostics;
using System.Runtime.InteropServices.JavaScript;
using System.Runtime.Versioning;
using Stello.Engine;

namespace Stello.Web.Worker;

/// <summary>The engine and the opening book inside the Web Worker (see wwwroot/js/engine-worker.js).</summary>
[SupportedOSPlatform("browser")]
public static partial class EngineWorker
{
    private static OpeningBook s_book = OpeningBook.CreateEmpty();
    private static ComputerPlayer? s_computer;

    private static ComputerPlayer Computer =>
        s_computer ?? throw new InvalidOperationException("The engine worker has not been initialised.");

    [JSExport]
    public static void Init(byte[] book, int hashBits)
    {
        using var stream = new MemoryStream(book);
        s_book = OpeningBook.Load(stream);
        s_computer = new ComputerPlayer(new SearchEngine(hashBits), s_book, new Random());
    }

    [JSExport]
    public static string ChooseMove(string request)
    {
        (Board board, Player player, SearchLimits limits) = EngineProtocol.ReadRequest(request);
        return EngineProtocol.WriteResult(Computer.ChooseMove(board, player, limits, new ProgressPoster()));
    }

    [JSExport]
    public static void ResetBookTracker() => Computer.BookTracker.Reset();

    /// <param name="moves">The played moves separated by spaces, e.g. "f5 d6 pass".</param>
    /// <returns>The number of positions in the book.</returns>
    [JSExport]
    public static int AddGame(string moves, int result)
    {
        List<Move> played = moves.Split(' ', StringSplitOptions.RemoveEmptyEntries)
            .Select(text => Move.TryParse(text, out Move? move) ? move.Value : throw new FormatException($"Not a move: {text}"))
            .ToList();

        // Adding a game does not search, so the limits do not matter.
        new BookLearner(s_book, new SearchEngine(10), SearchLimits.FixedDepth(1), new Random()).AddGame(played, (GameResult)result);
        return s_book.NodeCount;
    }

    [JSExport]
    public static byte[] GetBook()
    {
        using var stream = new MemoryStream();
        s_book.Save(stream);
        return stream.ToArray();
    }

    [JSImport("postProgress", "engine-worker")]
    private static partial void PostProgress(string json);

    // Posting every report would flood the page; new depths and new best moves always go through.
    private sealed class ProgressPoster : IProgress<SearchInfo>
    {
        private const long IntervalMs = 100;

        private readonly Stopwatch _clock = Stopwatch.StartNew();
        private long _lastPostMs = -IntervalMs;
        private int _lastDepth = -1;
        private Square? _lastBest;

        public void Report(SearchInfo info)
        {
            long now = _clock.ElapsedMilliseconds;
            if (info.Depth == _lastDepth && info.BestMove == _lastBest && now - _lastPostMs < IntervalMs)
            {
                return;
            }

            _lastDepth = info.Depth;
            _lastBest = info.BestMove;
            _lastPostMs = now;
            PostProgress(EngineProtocol.WriteProgress(info));
        }
    }
}
