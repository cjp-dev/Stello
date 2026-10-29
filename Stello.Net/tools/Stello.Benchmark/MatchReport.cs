using System.Globalization;
using System.Text;
using Stello.Engine;

namespace Stello.Benchmark;

/// <summary>How one side played over the whole match.</summary>
/// <param name="MidgameDepth">The average depth of the moves that ended in the midgame search.</param>
/// <param name="FirstSolve">The average number of empty squares at which the side first solved a game, over the games it solved.</param>
internal sealed record SideSummary(string Name, string Settings, int SearchedMoves, double NodesPerSecond, double MidgameDepth, double FirstSolve, double SecondsPerGame)
{
    public static SideSummary Of(MatchSide side, IEnumerable<SideStats> games)
    {
        List<SideStats> list = games.ToList();
        long nodes = list.Sum(s => s.Nodes);
        double seconds = list.Sum(s => s.Time.TotalSeconds);
        int midgameMoves = list.Sum(s => s.MidgameMoves);
        List<SideStats> solved = list.Where(s => s.FirstSolve > 0).ToList();
        return new SideSummary(
            side.Name,
            MatchCommands.Describe(side),
            list.Sum(s => s.Searched),
            seconds > 0 ? nodes / seconds : 0,
            midgameMoves > 0 ? (double)list.Sum(s => s.MidgameDepth) / midgameMoves : 0,
            solved.Count > 0 ? solved.Average(s => s.FirstSolve) : 0,
            list.Count > 0 ? seconds / list.Count : 0);
    }
}

/// <param name="DiscsBWhenAFirst">Side B's disc difference in the game where side A played the side to move.</param>
internal sealed record PairResult(string Start, int DiscsBWhenAFirst, int DiscsBWhenBFirst);

/// <summary>The result of a match, from side B's point of view, with 95 % intervals over the pairs of games.</summary>
internal sealed record MatchReport(
    SideSummary A,
    SideSummary B,
    int Games,
    int WinsB,
    int DrawsB,
    int LossesB,
    double PointsB,
    double Score,
    double ScoreLow,
    double ScoreHigh,
    double Elo,
    double EloLow,
    double EloHigh,
    double LikelihoodOfSuperiority,
    double DiscsB,
    double DiscsMargin,
    string Duration,
    IReadOnlyList<PairResult> Pairs)
{
    public static MatchReport Of(MatchSide a, MatchSide b, IReadOnlyList<MatchPair> pairs, TimeSpan duration)
    {
        MatchSummary summary = MatchSummary.Of(pairs);
        List<MatchGame> games = pairs.SelectMany(p => new[] { p.AFirst, p.BFirst }).ToList();

        double[] scores = pairs.Select(p => p.PointsB / 2).ToArray();
        double scoreError = StandardError(scores);
        double[] discs = pairs.Select(p => (p.AFirst.DiscsB + p.BFirst.DiscsB) / 2.0).ToArray();

        return new MatchReport(
            SideSummary.Of(a, games.Select(g => g.StatsA)),
            SideSummary.Of(b, games.Select(g => g.StatsB)),
            games.Count,
            games.Count(g => g.DiscsB > 0),
            games.Count(g => g.DiscsB == 0),
            games.Count(g => g.DiscsB < 0),
            summary.PointsB,
            summary.Score,
            summary.Low,
            summary.High,
            EloOf(summary.Score),
            EloOf(summary.Low),
            EloOf(summary.High),
            scoreError > 0 ? NormalCdf((summary.Score - 0.5) / scoreError) : summary.Score > 0.5 ? 1 : summary.Score < 0.5 ? 0 : 0.5,
            summary.AverageDiscsB,
            1.96 * StandardError(discs),
            Program.Duration(duration),
            pairs.Select(p => new PairResult(p.Start, p.AFirst.DiscsB, p.BFirst.DiscsB)).ToList());
    }

    public string Markdown(bool completed)
    {
        var text = new StringBuilder();
        text.AppendLine(CultureInfo.InvariantCulture, $"### Match: `{B.Name}` (B) against `{A.Name}` (A), {Games} games{(completed ? "" : " (stopped early)")}");
        text.AppendLine();
        text.AppendLine("| | A | B |");
        text.AppendLine("|---|---|---|");
        text.AppendLine(CultureInfo.InvariantCulture, $"| Settings | {A.Settings} | {B.Settings} |");
        text.AppendLine(CultureInfo.InvariantCulture, $"| Points | {Games - PointsB:0.#} | **{PointsB:0.#}** ({Score:P1}) |");
        text.AppendLine(CultureInfo.InvariantCulture, $"| Wins / draws / losses | +{LossesB} ={DrawsB} −{WinsB} | +{WinsB} ={DrawsB} −{LossesB} |");
        text.AppendLine(CultureInfo.InvariantCulture, $"| Midgame depth (average) | {A.MidgameDepth:0.00} | {B.MidgameDepth:0.00} |");
        text.AppendLine(CultureInfo.InvariantCulture, $"| First solve (empty squares, average) | {A.FirstSolve:0.0} | {B.FirstSolve:0.0} |");
        text.AppendLine(CultureInfo.InvariantCulture, $"| Nodes per second | {A.NodesPerSecond / 1e6:0.00}M | {B.NodesPerSecond / 1e6:0.00}M |");
        text.AppendLine(CultureInfo.InvariantCulture, $"| Thinking time per game | {A.SecondsPerGame:0.0} s | {B.SecondsPerGame:0.0} s |");
        text.AppendLine();
        text.AppendLine(CultureInfo.InvariantCulture,
            $"B scored **{Score:P1}** (95 % interval {ScoreLow:P1} to {ScoreHigh:P1}), Elo **{Signed(Elo)}** ({Signed(EloLow)} to {Signed(EloHigh)}), " +
            $"likelihood of superiority {LikelihoodOfSuperiority:P1}; discs per game {DiscsB:+0.0;-0.0;0.0} ± {DiscsMargin:0.0}. Wall time {Duration}.");
        text.Append(ScoreLow > 0.5 ? "B is stronger." : ScoreHigh < 0.5 ? "A is stronger." : "No significant difference.");
        return text.ToString();
    }

    private static string Signed(double elo) =>
        double.IsInfinity(elo) ? (elo > 0 ? "+∞" : "−∞") : elo.ToString("+0;-0;0", CultureInfo.InvariantCulture);

    /// <summary>The Elo difference that gives the expected score <paramref name="score"/>.</summary>
    internal static double EloOf(double score) =>
        score <= 0 ? double.NegativeInfinity : score >= 1 ? double.PositiveInfinity : -400 * Math.Log10(1 / score - 1);

    private static double StandardError(double[] values)
    {
        if (values.Length < 2)
        {
            return 0;
        }

        double mean = values.Average();
        double variance = values.Sum(v => (v - mean) * (v - mean)) / (values.Length - 1);
        return Math.Sqrt(variance / values.Length);
    }

    // Abramowitz and Stegun 7.1.26; accurate to about 1e-7, plenty for a likelihood.
    private static double NormalCdf(double x)
    {
        double z = Math.Abs(x) / Math.Sqrt(2);
        double t = 1 / (1 + 0.3275911 * z);
        double erf = 1 - (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t * Math.Exp(-z * z);
        return x >= 0 ? (1 + erf) / 2 : (1 - erf) / 2;
    }
}
