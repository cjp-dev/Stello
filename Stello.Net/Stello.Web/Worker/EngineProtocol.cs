using System.Numerics;
using System.Text.Json;
using System.Text.Json.Serialization;
using Stello.Engine;

namespace Stello.Web.Worker;

// Messages between the page and the engine worker; plain values only, so 64-bit boards and ticks stay exact.
internal sealed record MoveRequest(ulong Black, ulong White, Player Player, TimeControlMode Mode, int Depth, long TimeTicks);

internal sealed record MoveResponse(int Square, int Score, ScoreKind Kind, int Depth, long Nodes, long Evaluations, long ElapsedTicks);

internal sealed record ProgressReport(int Depth, int CurrentMove, int BestMove, int Score, ScoreKind Kind, long Nodes, long Evaluations, long ElapsedTicks);

[JsonSerializable(typeof(MoveRequest))]
[JsonSerializable(typeof(MoveResponse))]
[JsonSerializable(typeof(ProgressReport))]
internal sealed partial class EngineProtocolJson : JsonSerializerContext;

internal static class EngineProtocol
{
    public static string WriteRequest(Board board, Player player, SearchLimits limits) => JsonSerializer.Serialize(
        new MoveRequest(board.Black, board.White, player, limits.Mode, limits.Depth, limits.Time.Ticks),
        EngineProtocolJson.Default.MoveRequest);

    public static (Board Board, Player Player, SearchLimits Limits) ReadRequest(string json)
    {
        MoveRequest request = JsonSerializer.Deserialize(json, EngineProtocolJson.Default.MoveRequest)!;
        TimeSpan time = TimeSpan.FromTicks(request.TimeTicks);
        SearchLimits limits = request.Mode switch
        {
            TimeControlMode.FixedDepth => SearchLimits.FixedDepth(request.Depth),
            TimeControlMode.TimePerMove => SearchLimits.TimePerMove(time),
            TimeControlMode.TimePerGame => SearchLimits.TimePerGame(time),
            _ => SearchLimits.Solve,
        };
        return (new Board(request.Black, request.White), request.Player, limits);
    }

    public static string WriteResult(SearchResult result) => JsonSerializer.Serialize(
        new MoveResponse(
            result.Move.Square?.Index ?? -1,
            result.Score,
            result.Kind,
            result.Depth,
            result.Nodes,
            result.Evaluations,
            result.Elapsed.Ticks),
        EngineProtocolJson.Default.MoveResponse);

    public static SearchResult ReadResult(string json)
    {
        MoveResponse response = JsonSerializer.Deserialize(json, EngineProtocolJson.Default.MoveResponse)!;
        return new SearchResult(
            response.Square >= 0 ? new Move(new Square(response.Square)) : Move.Pass,
            response.Score,
            response.Kind,
            response.Depth,
            response.Nodes,
            response.Evaluations,
            TimeSpan.FromTicks(response.ElapsedTicks));
    }

    public static string WriteProgress(SearchInfo info) => JsonSerializer.Serialize(
        new ProgressReport(
            info.Depth,
            info.CurrentMove.Index,
            info.BestMove?.Index ?? -1,
            info.Score,
            info.Kind,
            info.Nodes,
            info.Evaluations,
            info.Elapsed.Ticks),
        EngineProtocolJson.Default.ProgressReport);

    public static SearchInfo ReadProgress(string json)
    {
        ProgressReport report = JsonSerializer.Deserialize(json, EngineProtocolJson.Default.ProgressReport)!;
        return new SearchInfo(
            report.Depth,
            new Square(report.CurrentMove),
            report.BestMove >= 0 ? new Square(report.BestMove) : null,
            report.Score,
            report.Kind,
            report.Nodes,
            report.Evaluations,
            TimeSpan.FromTicks(report.ElapsedTicks));
    }

    /// <summary>What "move now" plays when the worker is stopped: the best move reported so far.</summary>
    public static SearchResult MoveNowResult(Board board, Player player, SearchInfo? last, TimeSpan elapsed)
    {
        ulong legal = board.LegalMoves(player);
        Square? square = last?.BestMove ?? last?.CurrentMove
            ?? (legal != 0 ? new Square(BitOperations.TrailingZeroCount(legal)) : null);
        return new SearchResult(
            square is { } s ? new Move(s) : Move.Pass,
            last?.Score ?? 0,
            last?.Kind ?? ScoreKind.None,
            last?.Depth ?? 0,
            last?.Nodes ?? 0,
            last?.Evaluations ?? 0,
            elapsed);
    }
}
