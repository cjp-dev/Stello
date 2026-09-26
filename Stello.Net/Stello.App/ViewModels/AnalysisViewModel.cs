using System.Globalization;
using CommunityToolkit.Mvvm.ComponentModel;
using Stello.Engine;

namespace Stello.Net.ViewModels;

/// <summary>The search shown while the computer thinks (C++: CAnalyse, IDD_ANALYSE).</summary>
public sealed partial class AnalysisViewModel : ObservableObject
{
    private const int WinScore = 32600;

    [ObservableProperty]
    private string _move = "";

    [ObservableProperty]
    private string _depth = "";

    [ObservableProperty]
    private string _value = "";

    [ObservableProperty]
    private string _bestMove = "";

    [ObservableProperty]
    private string _nodes = "";

    [ObservableProperty]
    private string _evaluations = "";

    [ObservableProperty]
    private string _time = "";

    public void Clear()
    {
        Move = Depth = Value = BestMove = Nodes = Evaluations = Time = "";
    }

    public void Update(SearchInfo info)
    {
        Move = info.CurrentMove.ToString();
        Depth = FormatDepth(info.Depth, info.Kind);
        Value = FormatScore(info.Score, info.Kind);
        BestMove = info.BestMove?.ToString() ?? "";
        Nodes = info.Nodes.ToString("N0", CultureInfo.CurrentCulture);
        Evaluations = info.Evaluations.ToString("N0", CultureInfo.CurrentCulture);
        Time = FormatTime(info.Elapsed);
    }

    public void Update(SearchResult result, TimeSpan elapsed)
    {
        Move = result.Move.ToString();
        BestMove = result.Move.ToString();
        Depth = FormatDepth(result.Depth, result.Kind);
        Value = FormatScore(result.Score, result.Kind);
        Nodes = result.Nodes.ToString("N0", CultureInfo.CurrentCulture);
        Evaluations = result.Evaluations.ToString("N0", CultureInfo.CurrentCulture);
        Time = FormatTime(elapsed);
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
