// SPDX-License-Identifier: GPL-3.0-only

using System.Diagnostics;
using Stello.BookTool;
using Stello.Egaroucid;
using Stello.Engine;
using Stello.Engine.Search;
using Stello.Engine.Benchmark;
using SearchResult = Stello.Engine.SearchResult;

namespace Stello.Benchmark;

/// <summary>
/// The engine a benchmark searches with (<c>--engine stello|stello-v1|egaroucid</c>): one search per position, each with empty hash
/// tables, on 1 thread (Stello: <c>--threads N</c>).
/// </summary>
/// <param name="Engine">stello, stello-v1 or egaroucid.</param>
/// <param name="Limits">The limits as text, for the report.</param>
/// <param name="HashBits">Stello: two tables of 2^bits slots of two entries; Egaroucid: one table of 2^bits slots.</param>
internal sealed record BenchSearcher(string Engine, string Limits, int HashBits, Func<BenchmarkPosition, SearchResult> Search)
{
    public string Description => $"{Engine}, {Limits}, hash bits {HashBits}";

    /// <summary>
    /// The searcher for <paramref name="limits"/>, which is <see cref="SearchLimits.Solve"/> or a fixed depth. For
    /// Egaroucid, a fixed depth is searched with <c>--mpc</c> (100 = no selectivity, the default), or <c>--level</c>
    /// replaces the depth.
    /// </summary>
    /// <exception cref="ArgumentException">An option is not valid.</exception>
    public static BenchSearcher Create(Options options, SearchLimits limits, string limitText)
    {
        string engine = options.Text("engine") ?? "stello";
        if (engine != "stello" && options.Text("threads") is not null)
        {
            throw new ArgumentException("--threads is only for --engine stello.");
        }

        switch (engine)
        {
            case "stello":
                if (options.Text("mpc") is not null || options.Text("level") is not null)
                {
                    throw new ArgumentException("--mpc and --level are only for --engine egaroucid.");
                }

                int bits = options.Number("hash-bits", SearchLimits.DefaultHashBits, 10, 26);
                string? evalFile = options.Text("eval");
                SearchOptions searchOptions = evalFile is null or "default" ? SearchOptions.Default : MatchCommands.PatternOptions(evalFile);
                if (evalFile is not null and not "default")
                {
                    limitText += $", evaluation {Path.GetFileName(evalFile)}";
                }

                // --side: the search switches of a match side, e.g. "order=4,killers=off".
                if (options.Text("side") is { } sideText)
                {
                    MatchSide side = MatchCommands.ParseSide($"depth=1,{sideText}");
                    searchOptions = (side.Options ?? SearchOptions.Default) with { Pattern = searchOptions.Pattern ?? side.Options?.Pattern };
                    limitText += $", {sideText}";
                }

                // --selective and --probcut-pct, as for stello-v2: the parity runs of the Rust engine against Stello 2.0.
                (searchOptions, limitText) = SearchSwitches(options, searchOptions, limitText);

                // --threads: the threads share the hash tables; with more than 1 the nodes vary from run to run.
                int threads = options.Number("threads", 1, 1, SearchLimits.MaxThreads);
                if (threads > 1)
                {
                    limits = limits with { Threads = threads };
                    limitText += $", {threads} threads";
                }

                var stello = new SearchEngine(bits, searchOptions);

                return new BenchSearcher(engine, limitText, bits, position =>
                {
                    stello.ClearHash();
                    return stello.Search(position.Board, position.Player, limits);
                });

            case "stello-v1":
                if (options.Text("mpc") is not null || options.Text("level") is not null || options.Text("eval") is not null)
                {
                    throw new ArgumentException("--mpc and --level are only for --engine egaroucid, --eval only for --engine stello.");
                }

                int v1Bits = options.Number("hash-bits", SearchLimits.DefaultHashBits, 10, 26);
                var v1 = new Stello.Engine.V1.SearchEngine(v1Bits);
                return new BenchSearcher(engine, limitText, v1Bits, position =>
                {
                    v1.ClearHash();
                    return v1.Search(position.Board, position.Player, limits);
                });

            case "stello-v2":
                if (options.Text("mpc") is not null || options.Text("level") is not null || options.Text("side") is not null)
                {
                    throw new ArgumentException("--mpc and --level are only for --engine egaroucid, --side only for --engine stello.");
                }

                int v2Bits = options.Number("hash-bits", SearchLimits.DefaultHashBits, 10, 26);
                string? v2Eval = options.Text("eval");
                var v2Options = v2Eval is null or "default"
                    ? Stello.Engine.V2.Search.SearchOptions.Default
                    : new Stello.Engine.V2.Search.SearchOptions { Pattern = Stello.Engine.V2.Evaluation.EvalWeights.Load(v2Eval) };
                if (v2Eval is not null and not "default")
                {
                    limitText += $", evaluation {Path.GetFileName(v2Eval)}";
                }

                // --selective off (the plain search of the trainer's mpc) and --probcut-pct: Stello 2.0's search switches,
                // for the Rust engine's parity runs (stello mid-bench and --engine stello take the same options).
                (bool selective, int? probCutPercent, limitText) = SwitchValues(options, limitText);
                v2Options = v2Options with { Selective = selective, ProbCutPercent = probCutPercent ?? v2Options.ProbCutPercent };

                var v2 = new Stello.Engine.V2.SearchEngine(v2Bits, v2Options);
                return new BenchSearcher(engine, limitText, v2Bits, position =>
                {
                    v2.ClearHash();
                    return v2.Search(position.Board, position.Player, limits);
                });

            case "egaroucid":
                if (options.Text("eval") is not null)
                {
                    throw new ArgumentException("--eval is only for --engine stello.");
                }

                MatchCommands.LoadEgaroucid();
                int tableBits = options.Number("hash-bits", MatchCommands.EgaroucidDefaultTableBits, 10, HashTables.DefaultHashLevel);
                var context = new SearchContext(tableBits);
                Func<Stello.Egaroucid.Board, Stello.Egaroucid.SearchResult> search;
                if (limits.Mode == TimeControlMode.Solve)
                {
                    search = board => Ai.Search(context, board, Level.MAX_LEVEL, useMultiThread: false);
                }
                else if (options.Text("level") is not null)
                {
                    int level = options.Number("level", 21, 1, Level.MAX_LEVEL);
                    limitText = $"level {level}";
                    search = board => Ai.Search(context, board, level, useMultiThread: false);
                }
                else
                {
                    int mpc = MpcLevel(options.Text("mpc") ?? "100");
                    limitText += $", MPC {Level.SelectivityPercentage[mpc].ToString(System.Globalization.CultureInfo.InvariantCulture)} %";
                    search = board => Ai.TreeSearchLegal(context, board, -Common.SCORE_MAX, Common.SCORE_MAX, limits.Depth, mpc, board.GetLegal(),
                        useMultiThread: false, SearchConst.TIME_LIMIT_INF, new SearchingFlag(), log: null);
                }

                return new BenchSearcher(engine, limitText, tableBits, position =>
                {
                    context.Clear();
                    long started = Stopwatch.GetTimestamp();
                    Stello.Egaroucid.SearchResult result = search(EgaroucidPlayer.ToEgaroucid(position.Board, position.Player));
                    return EgaroucidPlayer.ToStello(result, position.Board, position.Player, Stopwatch.GetElapsedTime(started));
                });

            default:
                throw new ArgumentException("--engine must be stello, stello-v2, stello-v1 or egaroucid.");
        }
    }

    private static int MpcLevel(string text)
    {
        string[] names = Level.SelectivityPercentage.ToArray().Select(p => p.ToString(System.Globalization.CultureInfo.InvariantCulture)).ToArray();
        int level = Array.IndexOf(names, text);
        return level >= 0 ? level : throw new ArgumentException($"--mpc must be one of {string.Join(", ", names)}.");
    }

    private static (SearchOptions Options, string LimitText) SearchSwitches(Options options, SearchOptions searchOptions, string limitText)
    {
        (bool selective, int? probCutPercent, limitText) = SwitchValues(options, limitText);
        return (searchOptions with { Selective = selective, ProbCutPercent = probCutPercent ?? searchOptions.ProbCutPercent }, limitText);
    }

    // --selective on|off and --probcut-pct N.
    private static (bool Selective, int? ProbCutPercent, string LimitText) SwitchValues(Options options, string limitText)
    {
        bool selective = options.Text("selective") switch
        {
            null or "on" => true,
            "off" => false,
            _ => throw new ArgumentException("--selective must be on or off."),
        };
        if (!selective)
        {
            limitText += ", selective off";
        }

        int? percent = null;
        if (options.Text("probcut-pct") is not null)
        {
            percent = options.Number("probcut-pct", 100, 1, 1000);
            limitText += $", probcut {percent} %";
        }

        return (selective, percent, limitText);
    }
}
