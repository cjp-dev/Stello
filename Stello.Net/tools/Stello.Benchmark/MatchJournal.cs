using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using Stello.Engine;

namespace Stello.Benchmark;

internal sealed record MatchRunSettings(string SideA, string SideB, int StartCount, string StartsHash)
{
    public static MatchRunSettings Create(string sideA, string sideB, IReadOnlyList<string> starts)
    {
        string positions = string.Join('\n', starts);
        string hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(positions)));
        return new MatchRunSettings(sideA, sideB, starts.Count, hash);
    }
}

internal sealed record MatchGameData(Player ColourB, int DiscsB, SideStats StatsA, SideStats StatsB)
{
    public static MatchGameData From(MatchGame game) => new(game.ColourB, game.DiscsB, game.StatsA, game.StatsB);

    public MatchGame ToGame() => new(ColourB, [], DiscsB) { StatsA = StatsA, StatsB = StatsB };
}

internal sealed record MatchGameRecord(
    int Version,
    MatchRunSettings Settings,
    int StartIndex,
    int GameNumber,
    string Start,
    MatchGameData Game);

internal sealed class MatchJournal : IDisposable
{
    public const int FormatVersion = 1;

    private static readonly JsonSerializerOptions Json = new()
    {
        NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals,
    };

    private readonly string _path;
    private readonly MatchRunSettings _settings;
    private readonly IReadOnlyList<string> _starts;
    private readonly Player[] _firstPlayer;
    private readonly Dictionary<(int StartIndex, int GameNumber), MatchGameRecord> _games = [];
    private readonly object _gate = new();
    private FileStream? _stream;

    private MatchJournal(string path, MatchRunSettings settings, IReadOnlyList<string> starts, FileStream? stream = null)
    {
        _path = path;
        _settings = settings;
        _starts = starts;
        _stream = stream;
        _firstPlayer = starts.Select(start => BookTextFormat.Replay(start).Player).ToArray();
    }

    public static MatchJournal Create(string path, MatchRunSettings settings, IReadOnlyList<string> starts)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
        if (File.Exists(path) && new FileInfo(path).Length == 0)
        {
            return new MatchJournal(path, settings, starts, new FileStream(path, FileMode.Open, FileAccess.Write, FileShare.Read));
        }

        try
        {
            return new MatchJournal(path, settings, starts, new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read));
        }
        catch (IOException exception) when (File.Exists(path))
        {
            throw new ArgumentException($"The match game log already exists: {path}. Continue it with --resume, or choose another --name.", exception);
        }
    }

    public static MatchJournal Resume(string path, MatchRunSettings settings, IReadOnlyList<string> starts)
    {
        if (!File.Exists(path))
        {
            throw new ArgumentException($"Cannot resume: the match game log does not exist: {path}.");
        }

        var journal = new MatchJournal(path, settings, starts);
        try
        {
            journal.Load();
            if (journal._games.Count == 0)
            {
                throw new ArgumentException("Cannot resume: the match game log contains no completed games. Start a fresh match without --resume.");
            }

            journal._stream = new FileStream(path, FileMode.Append, FileAccess.Write, FileShare.Read);
            return journal;
        }
        catch
        {
            journal.Dispose();
            throw;
        }
    }

    public MatchGame?[] AFirstGames()
    {
        lock (_gate)
        {
            var games = new MatchGame?[_starts.Count];
            foreach (MatchGameRecord record in _games.Values.Where(record => record.GameNumber == 0))
            {
                games[record.StartIndex] = record.Game.ToGame();
            }

            return games;
        }
    }

    public MatchGame?[] BFirstGames()
    {
        lock (_gate)
        {
            var games = new MatchGame?[_starts.Count];
            foreach (MatchGameRecord record in _games.Values.Where(record => record.GameNumber == 1))
            {
                games[record.StartIndex] = record.Game.ToGame();
            }

            return games;
        }
    }

    public List<MatchPair> Pairs()
    {
        lock (_gate)
        {
            var pairs = new List<MatchPair>();
            for (int i = 0; i < _starts.Count; i++)
            {
                if (_games.TryGetValue((i, 0), out MatchGameRecord? first)
                    && _games.TryGetValue((i, 1), out MatchGameRecord? second))
                {
                    pairs.Add(new MatchPair(_starts[i], first.Game.ToGame(), second.Game.ToGame()));
                }
            }

            return pairs;
        }
    }

    public void Append(MatchGameRecord record)
    {
        ValidateRecord(record);
        byte[] json = JsonSerializer.SerializeToUtf8Bytes(record, Json);
        lock (_gate)
        {
            var key = (record.StartIndex, record.GameNumber);
            if (_games.ContainsKey(key))
            {
                throw new InvalidDataException($"The match game log already contains game {record.GameNumber + 1} for start {record.StartIndex + 1}.");
            }

            FileStream stream = _stream ?? throw new ObjectDisposedException(nameof(MatchJournal));
            stream.Write(json);
            stream.WriteByte((byte)'\n');
            stream.Flush(flushToDisk: true);
            _games.Add(key, record);
        }
    }

    public void Dispose() => _stream?.Dispose();

    private void Load()
    {
        byte[] bytes = File.ReadAllBytes(_path);
        int lastLineEnd = Array.LastIndexOf(bytes, (byte)'\n') + 1;
        if (lastLineEnd < bytes.Length)
        {
            using var truncate = new FileStream(_path, FileMode.Open, FileAccess.Write, FileShare.Read);
            truncate.SetLength(lastLineEnd);
            truncate.Flush(flushToDisk: true);
            Console.Error.WriteLine($"Discarded an incomplete final line from the match game log: {_path}");
        }

        var utf8 = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true);
        int lineNumber = 0;
        int start = 0;
        while (start < lastLineEnd)
        {
            int lineEnd = Array.IndexOf(bytes, (byte)'\n', start, lastLineEnd - start);
            if (lineEnd < 0)
            {
                throw new InvalidDataException($"The match game log has an invalid line ending at byte {start}.");
            }

            lineNumber++;
            int lineLength = lineEnd - start;
            if (lineLength > 0 && bytes[lineEnd - 1] == (byte)'\r')
            {
                lineLength--;
            }

            try
            {
                string line = utf8.GetString(bytes, start, lineLength);
                MatchGameRecord record = JsonSerializer.Deserialize<MatchGameRecord>(line, Json)
                    ?? throw new InvalidDataException("The line is empty.");
                ValidateRecord(record);
                if (!_games.TryAdd((record.StartIndex, record.GameNumber), record))
                {
                    throw new InvalidDataException($"The log repeats game {record.GameNumber + 1} for start {record.StartIndex + 1}.");
                }
            }
            catch (Exception exception) when (exception is JsonException or DecoderFallbackException or InvalidDataException or ArgumentException)
            {
                throw new InvalidDataException($"Cannot resume from line {lineNumber} of the match game log: {exception.Message}", exception);
            }

            start = lineEnd + 1;
        }
    }

    private void ValidateRecord(MatchGameRecord record)
    {
        if (record.Version != FormatVersion)
        {
            throw new InvalidDataException($"The match game log has unsupported format version {record.Version}.");
        }

        EnsureCompatible(record.Settings, _settings);
        if (record.StartIndex < 0 || record.StartIndex >= _starts.Count || record.GameNumber is < 0 or > 1
            || record.Start != _starts[record.StartIndex])
        {
            throw new InvalidDataException("The match game log contains a game for a different start position.");
        }

        if (record.Game is null || record.Game.ColourB != (record.GameNumber == 0 ? _firstPlayer[record.StartIndex].Opponent() : _firstPlayer[record.StartIndex])
            || record.Game.DiscsB is < -64 or > 64)
        {
            throw new InvalidDataException("The match game log contains an invalid game result.");
        }

        ValidateStats(record.Game.StatsA);
        ValidateStats(record.Game.StatsB);
    }

    private static void ValidateStats(SideStats stats)
    {
        if (stats.Searched < 0 || stats.Nodes < 0 || stats.Time < TimeSpan.Zero || stats.MidgameMoves < 0
            || stats.MidgameDepth < 0 || stats.FirstSolve < 0 || stats.Longest < TimeSpan.Zero)
        {
            throw new InvalidDataException("The match game log contains invalid search statistics.");
        }
    }

    private static void EnsureCompatible(MatchRunSettings? saved, MatchRunSettings expected)
    {
        if (saved is null)
        {
            throw new InvalidDataException("The match game log has no run settings.");
        }

        if (saved.SideA != expected.SideA || saved.SideB != expected.SideB)
        {
            throw new ArgumentException("side A or B settings, time limits, or Egaroucid book seeds differ from the saved match");
        }

        if (saved.StartCount != expected.StartCount || saved.StartsHash != expected.StartsHash)
        {
            throw new ArgumentException("the start positions differ from the saved match");
        }
    }
}
