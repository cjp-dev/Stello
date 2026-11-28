using System.Globalization;
using System.Text;
using Stello.Engine;

namespace Stello.Benchmark;

/// <summary>How one side played over the whole match.</summary>
/// <param name="MidgameDepth">The average depth of the moves that ended in the midgame search.</param>
/// <param name="FirstSolve">The average number of empty squares at which the side first solved a game, over the games it solved.</param>
/// <param name="MsPerSearchedMove">The average wall time of a searched move.</param>
/// <param name="LongestMoveMs">The longest wall time of a move in the match.</param>
internal sealed record SideSummary(
    string Name, string Settings, int SearchedMoves, double NodesPerSecond, double MidgameDepth, double FirstSolve, double SecondsPerGame,
    double MsPerSearchedMove = 0, double LongestMoveMs = 0)
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
            list.Count > 0 ? seconds / list.Count : 0,
            list.Sum(s => s.Searched) > 0 ? 1000 * seconds / list.Sum(s => s.Searched) : 0,
            list.Count > 0 ? list.Max(s => s.Longest.TotalMilliseconds) : 0);
    }
}

/// <param name="DiscsBWhenAFirst">Side B's disc difference in the game where side A played the side to move.</param>
internal sealed record PairResult(
    string Start,
    int DiscsBWhenAFirst,
    int DiscsBWhenBFirst,
    double ElapsedSecondsAWhenAFirst = 0,
    double ElapsedSecondsBWhenAFirst = 0,
    ClockOverrunSide OverrunWhenAFirst = ClockOverrunSide.None,
    double ElapsedSecondsAWhenBFirst = 0,
    double ElapsedSecondsBWhenBFirst = 0,
    ClockOverrunSide OverrunWhenBFirst = ClockOverrunSide.None);

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
    IReadOnlyList<PairResult> Pairs,
    bool HasGameClock = false,
    int OverrunsA = 0,
    int OverrunsB = 0,
    double FlagAdjustedPointsB = 0,
    double FlagAdjustedScore = 0.5,
    double FlagAdjustedScoreLow = 0.5,
    double FlagAdjustedScoreHigh = 0.5,
    double FlagAdjustedElo = 0,
    double FlagAdjustedEloLow = 0,
    double FlagAdjustedEloHigh = 0,
    double FlagAdjustedLos = 0.5)
{
    private readonly record struct ScoreOutcome(
        double PointsB, double Score, double ScoreLow, double ScoreHigh,
        double Elo, double EloLow, double EloHigh, double LikelihoodOfSuperiority);

    private static ScoreOutcome SummarizePairs(double[] pairPointsB)
    {
        if (pairPointsB.Length == 0)
        {
            return new ScoreOutcome(0, 0.5, 0.5, 0.5, 0, 0, 0, 0.5);
        }

        double[] scores = pairPointsB.Select(p => p / 2).ToArray();
        double mean = scores.Average();
        double error = StandardError(scores);
        double margin = 1.96 * error;
        double low = Math.Clamp(mean - margin, 0.0, 1.0);
        double high = Math.Clamp(mean + margin, 0.0, 1.0);
        double los = error > 0 ? NormalCdf((mean - 0.5) / error) : mean > 0.5 ? 1 : mean < 0.5 ? 0 : 0.5;
        return new ScoreOutcome(pairPointsB.Sum(), mean, low, high, EloOf(mean), EloOf(low), EloOf(high), los);
    }

    public static MatchReport Of(MatchSide a, MatchSide b, IReadOnlyList<MatchPair> pairs, TimeSpan duration)
    {
        List<MatchGame> games = pairs.SelectMany(p => new[] { p.AFirst, p.BFirst }).ToList();
        ScoreOutcome ordinary = SummarizePairs(pairs.Select(p => p.PointsB).ToArray());
        ScoreOutcome flagAdjusted = SummarizePairs(pairs.Select(p => p.FlagAdjustedPointsB).ToArray());
        double[] discs = pairs.Select(p => (p.AFirst.DiscsB + p.BFirst.DiscsB) / 2.0).ToArray();
        bool hasGameClock = a.Limits.Mode == TimeControlMode.TimePerGame || b.Limits.Mode == TimeControlMode.TimePerGame;

        return new MatchReport(
            SideSummary.Of(a, games.Select(g => g.StatsA)),
            SideSummary.Of(b, games.Select(g => g.StatsB)),
            games.Count,
            games.Count(g => g.DiscsB > 0),
            games.Count(g => g.DiscsB == 0),
            games.Count(g => g.DiscsB < 0),
            ordinary.PointsB,
            ordinary.Score,
            ordinary.ScoreLow,
            ordinary.ScoreHigh,
            ordinary.Elo,
            ordinary.EloLow,
            ordinary.EloHigh,
            ordinary.LikelihoodOfSuperiority,
            discs.Length > 0 ? discs.Average() : 0,
            1.96 * StandardError(discs),
            Program.Duration(duration),
            pairs.Select(p => new PairResult(
                p.Start,
                p.AFirst.DiscsB,
                p.BFirst.DiscsB,
                p.AFirst.ElapsedA.TotalSeconds,
                p.AFirst.ElapsedB.TotalSeconds,
                p.AFirst.FirstOverrun,
                p.BFirst.ElapsedA.TotalSeconds,
                p.BFirst.ElapsedB.TotalSeconds,
                p.BFirst.FirstOverrun)).ToList(),
            hasGameClock,
            games.Count(g => g.FirstOverrun == ClockOverrunSide.A),
            games.Count(g => g.FirstOverrun == ClockOverrunSide.B),
            flagAdjusted.PointsB,
            flagAdjusted.Score,
            flagAdjusted.ScoreLow,
            flagAdjusted.ScoreHigh,
            flagAdjusted.Elo,
            flagAdjusted.EloLow,
            flagAdjusted.EloHigh,
            flagAdjusted.LikelihoodOfSuperiority);
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
        text.AppendLine(CultureInfo.InvariantCulture, $"| Time per searched move (average / longest) | {A.MsPerSearchedMove:0} / {A.LongestMoveMs:0} ms | {B.MsPerSearchedMove:0} / {B.LongestMoveMs:0} ms |");
        if (HasGameClock || OverrunsA > 0 || OverrunsB > 0)
        {
            text.AppendLine(CultureInfo.InvariantCulture, $"| Clock overruns (first to exceed) | {OverrunsA} | {OverrunsB} |");
        }

        text.AppendLine();
        text.AppendLine(CultureInfo.InvariantCulture,
            $"B scored **{Score:P1}** (95 % interval {ScoreLow:P1} to {ScoreHigh:P1}), Elo **{Signed(Elo)}** ({Signed(EloLow)} to {Signed(EloHigh)}), " +
            $"likelihood of superiority {LikelihoodOfSuperiority:P1}; discs per game {DiscsB:+0.0;-0.0;0.0} ± {DiscsMargin:0.0}. Wall time {Duration}.");
        if (HasGameClock || OverrunsA > 0 || OverrunsB > 0)
        {
            text.AppendLine(CultureInfo.InvariantCulture,
                $"Flag-adjusted (first clock overrun loses): B **{FlagAdjustedPointsB:0.#}/{Games}** (**{FlagAdjustedScore:P1}**, " +
                $"95 % interval {FlagAdjustedScoreLow:P1} to {FlagAdjustedScoreHigh:P1}), Elo **{Signed(FlagAdjustedElo)}** ({Signed(FlagAdjustedEloLow)} to {Signed(FlagAdjustedEloHigh)}), " +
                $"likelihood of superiority {FlagAdjustedLos:P1}; first overruns A {OverrunsA}, B {OverrunsB}.");
        }

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
