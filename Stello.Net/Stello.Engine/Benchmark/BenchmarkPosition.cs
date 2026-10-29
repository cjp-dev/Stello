namespace Stello.Engine.Benchmark;

/// <summary>
/// A test position for the benchmarks (tools/Stello.Benchmark/Data). One position per line:
/// <c>&lt;id&gt; &lt;64 squares&gt; &lt;X|O&gt;[; A2:+38; C7:+36; ...]</c>. X is black and moves first; the optional list
/// gives the exact score of moves (the FFO and Edax <c>.obf</c> notation), best first.
/// </summary>
/// <param name="Score">The exact score with perfect play for the side to move, if known.</param>
/// <param name="BestMoves">The moves that reach <paramref name="Score"/>.</param>
internal sealed record BenchmarkPosition(string Id, Board Board, Player Player, int? Score, IReadOnlyList<Square> BestMoves)
{
    /// <exception cref="FormatException">A line is not a position in the format above.</exception>
    public static List<BenchmarkPosition> Read(TextReader reader)
    {
        var positions = new List<BenchmarkPosition>();
        while (reader.ReadLine() is { } line)
        {
            if (!string.IsNullOrWhiteSpace(line) && !line.TrimStart().StartsWith('#'))
            {
                positions.Add(Parse(line));
            }
        }

        return positions;
    }

    public static List<BenchmarkPosition> Load(string path)
    {
        using StreamReader reader = File.OpenText(path);
        return Read(reader);
    }

    /// <exception cref="FormatException">The line is not a position in the format above.</exception>
    public static BenchmarkPosition Parse(string line)
    {
        string[] parts = line.Split(';', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
        string[] head = parts[0].Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (head.Length != 3 || head[1].Length != 64)
        {
            throw new FormatException($"Not a position: {line}");
        }

        Board board = Board.Parse(head[1]);
        Player player = head[2] switch
        {
            "X" or "x" => Player.Black,
            "O" or "o" => Player.White,
            _ => throw new FormatException($"The side to move must be X or O: {line}"),
        };

        var scored = new List<(Square Move, int Score)>();
        foreach (string part in parts[1..])
        {
            string[] moveScore = part.Split(':');
            if (moveScore.Length != 2 || !Square.TryParse(moveScore[0].ToLowerInvariant(), out Square? move)
                || !int.TryParse(moveScore[1], System.Globalization.NumberStyles.AllowLeadingSign, System.Globalization.CultureInfo.InvariantCulture, out int score))
            {
                throw new FormatException($"Not a move with a score: {part}");
            }

            scored.Add((move.Value, score));
        }

        int? best = scored.Count > 0 ? scored.Max(s => s.Score) : null;
        Square[] bestMoves = scored.Where(s => s.Score == best).Select(s => s.Move).ToArray();
        return new BenchmarkPosition(head[0], board, player, best, bestMoves);
    }

    /// <summary>The line for this position, without scores.</summary>
    public string Format() =>
        $"{Id} {Board.ToString().Replace("\n", "", StringComparison.Ordinal)} {(Player == Player.Black ? 'X' : 'O')}";
}
