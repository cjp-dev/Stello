using System.Globalization;
using CommunityToolkit.Mvvm.ComponentModel;
using Stello.Engine;

namespace Stello.App.ViewModels;

/// <summary>The search shown while the computer thinks (C++: CAnalyse, IDD_ANALYSE).</summary>
public sealed partial class AnalysisViewModel : ObservableObject
{
    private const int WinScore = 32600;

    public event Action? Updated;

    [ObservableProperty]
    private string _move = "";

    [ObservableProperty]
    private string _depth = "";

    [ObservableProperty]
    private string _score = "";

    [ObservableProperty]
    private string _bestMove = "";

    [ObservableProperty]
    private string _nodes = "";

    [ObservableProperty]
    private string _speed = "";

    [ObservableProperty]
    private string _time = "";

    // Backward-compatibility aliases for existing WPF / tests
    public string Value => Score;
    public string Evaluations => Speed;

    public void Clear()
    {
        Move = Depth = Score = BestMove = Nodes = Speed = Time = "";
        Updated?.Invoke();
    }

    public void Update(SearchInfo info)
    {
        Move = info.CurrentMove.ToString();
        Depth = FormatDepth(info.Depth, info.Kind);
        Score = FormatScore(info.Score, info.Kind);
        BestMove = info.BestMove?.ToString() ?? "";
        Nodes = info.Nodes.ToString("N0", CultureInfo.CurrentCulture);
        Speed = FormatSpeed(info.Nodes, info.Elapsed);
        Time = FormatTime(info.Elapsed);
        Updated?.Invoke();
    }

    public void Update(SearchResult result, TimeSpan elapsed)
    {
        Move = result.Move.ToString();
        BestMove = result.Move.ToString();
        Depth = FormatDepth(result.Depth, result.Kind);
        Score = FormatScore(result.Score, result.Kind);
        Nodes = result.Nodes.ToString("N0", CultureInfo.CurrentCulture);
        Speed = FormatSpeed(result.Nodes, elapsed);
        Time = FormatTime(elapsed);
        Updated?.Invoke();
    }

    private static string FormatSpeed(long nodes, TimeSpan elapsed)
    {
        if (elapsed.TotalSeconds >= 0.05 && nodes > 0)
        {
            double nps = nodes / elapsed.TotalSeconds;
            return $"{((long)nps):N0} nps";
        }
        return "";
    }

    public static string FormatScore(int score, ScoreKind kind) => kind switch
    {
        ScoreKind.None => "",
        ScoreKind.Book => "Book",
        ScoreKind.WinLossDraw => score > 0 ? "Win" : score < 0 ? "Loss" : "Draw",
        ScoreKind.Exact => $"{score:+0;-0;0} discs",
        _ when score > WinScore => "Win",
        _ when score < -WinScore => "Loss",
        _ => score.ToString("+0;-0;0", CultureInfo.CurrentCulture),
    };

    private static string FormatDepth(int depth, ScoreKind kind) => kind switch
    {
        ScoreKind.None or ScoreKind.Book => "",
        ScoreKind.Heuristic => $"{depth} plies",
        _ => $"to the end ({depth} empty)",
    };

    private static string FormatTime(TimeSpan elapsed) => elapsed.ToString(@"m\:ss\.f", CultureInfo.CurrentCulture);
}
