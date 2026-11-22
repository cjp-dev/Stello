// SPDX-License-Identifier: GPL-3.0-only

using System.Diagnostics;
using System.Globalization;
using System.Numerics;
using System.Text;
using Stello.Engine;

namespace Stello.Benchmark;

/// <summary>
/// Native C Edax (<c>edax-reversi-master</c>) as a persistent per-worker subprocess <see cref="IMatchPlayer"/>
/// speaking <c>-xboard -q -book-usage off -ponder off</c>.
/// </summary>
internal sealed class EdaxMatchPlayer : IMatchPlayer, IDisposable
{
    public const int DefaultHashBits = 21;

    private readonly int _hashBits;
    private readonly int _threads;
    private readonly string _exePath;
    private readonly string _evalPath;
    private readonly List<string> _stderrTail = [];
    private readonly Lock _stderrLock = new();

    private Process? _process;
    private bool _synced;
    private Board _edaxBoard;
    private Player _edaxPlayer;
    private bool _sentLevel;
    private bool _disposed;

    public EdaxMatchPlayer(int hashBits = DefaultHashBits, int threads = 1)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(hashBits, 10);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(hashBits, 26);
        ArgumentOutOfRangeException.ThrowIfLessThan(threads, 1);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(threads, SearchLimits.MaxThreads);

        _hashBits = hashBits;
        _threads = threads;
        _exePath = ResolveExecutable();
        _evalPath = ResolveEvalFile();
        StartProcess();
    }

    public static string? FindExecutable()
    {
        string? fromEnv = Environment.GetEnvironmentVariable("EDAX_BIN");
        if (!string.IsNullOrWhiteSpace(fromEnv))
        {
            string full = Path.GetFullPath(fromEnv);
            return File.Exists(full) ? full : null;
        }

        string repo = Program.RepositoryFolder();
        string file = OperatingSystem.IsWindows()
            ? "wEdax-native.exe"
            : OperatingSystem.IsMacOS() ? "mEdax-native" : "lEdax-native";
        string candidate = Path.Combine(repo, "edax-reversi-master", "bin", file);
        return File.Exists(candidate) ? candidate : null;
    }

    public static string ResolveExecutable() =>
        FindExecutable() ?? throw new ArgumentException(
            "Could not find native Edax executable for engine=edax. Build edax-reversi-master into " +
            "edax-reversi-master/bin/wEdax-native.exe (or set EDAX_BIN).");

    public static string ResolveEvalFile()
    {
        string? fromEnv = Environment.GetEnvironmentVariable("EDAX_EVAL_FILE");
        if (!string.IsNullOrWhiteSpace(fromEnv))
        {
            string full = Path.GetFullPath(fromEnv);
            return File.Exists(full) ? full : throw new ArgumentException($"EDAX_EVAL_FILE points to a missing file: {full}");
        }

        string candidate = Path.Combine(Program.RepositoryFolder(), "edax-reversi-master", "bin", "data", "eval.dat");
        return File.Exists(candidate)
            ? candidate
            : throw new ArgumentException($"Could not find Edax eval.dat at {candidate} (or set EDAX_EVAL_FILE).");
    }

    public void NewGame()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        _synced = false;
        _sentLevel = false;
    }

    public SearchResult ChooseMove(Board board, Player player, SearchLimits limits, CancellationToken cancellationToken)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        cancellationToken.ThrowIfCancellationRequested();
        if (limits.Threads != _threads)
        {
            throw new InvalidOperationException($"Edax player was started with {_threads} threads, not {limits.Threads}.");
        }

        EnsureRunning();
        Process process = _process!;

        using CancellationTokenRegistration cancelReg = cancellationToken.Register(KillQuietly);
        try
        {
            SyncBoard(process, board, player);
            SendLimits(process, limits);

            long started = Stopwatch.GetTimestamp();
            SendLine(process, "go");
            process.StandardInput.Flush();

            int lastDepth = 0;
            int lastScore = 0;
            long lastNodes = 0;
            bool sawObserver = false;
            bool lastHasSelectivity = false;
            Square? chosen = null;

            while (true)
            {
                cancellationToken.ThrowIfCancellationRequested();
                string? line = process.StandardOutput.ReadLine();
                if (line is null)
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    throw new InvalidOperationException($"Edax exited unexpectedly during search.{StderrSummary()}");
                }

                string trimmed = line.Trim();
                if (trimmed.Length == 0 || trimmed.StartsWith("1-0", StringComparison.Ordinal) ||
                    trimmed.StartsWith("0-1", StringComparison.Ordinal) || trimmed.StartsWith("1/2-1/2", StringComparison.Ordinal) ||
                    trimmed.StartsWith("feature ", StringComparison.Ordinal) || trimmed.StartsWith("pong ", StringComparison.Ordinal))
                {
                    continue;
                }

                if (trimmed.StartsWith("move ", StringComparison.Ordinal))
                {
                    string moveText = trimmed[5..].Trim();
                    if (moveText.StartsWith("P@", StringComparison.OrdinalIgnoreCase))
                    {
                        moveText = moveText[2..];
                    }

                    if (!Square.TryParse(moveText, out Square? parsed) || parsed is null || !board.IsLegal(player, parsed.Value))
                    {
                        throw new InvalidOperationException($"Edax returned invalid/illegal move \"{trimmed}\" for {player}.");
                    }

                    chosen = parsed.Value;
                    break;
                }

                if (TryParseObserverLine(trimmed, out int depth, out int score, out long nodes, out bool hasSelectivity))
                {
                    lastDepth = depth;
                    lastScore = score;
                    lastNodes = nodes;
                    lastHasSelectivity = hasSelectivity;
                    sawObserver = true;
                }
            }

            TimeSpan elapsed = Stopwatch.GetElapsedTime(started);
            SendLine(process, "force");
            process.StandardInput.Flush();

            _edaxBoard = board.Play(player, chosen.Value);
            _edaxPlayer = player.Opponent();
            _synced = true;

            ScoreKind kind = !sawObserver && lastNodes == 0
                ? ScoreKind.None
                : !lastHasSelectivity && lastDepth >= board.EmptyCount
                    ? ScoreKind.Exact
                    : ScoreKind.Heuristic;

            return new SearchResult(new Move(chosen.Value), lastScore, kind, lastDepth, lastNodes, 0, elapsed);
        }
        catch (Exception) when (cancellationToken.IsCancellationRequested)
        {
            throw new OperationCanceledException(cancellationToken);
        }
    }

    public static string ToFen(Board board, Player toMove)
    {
        var sb = new StringBuilder(80);
        for (int rank = 7; rank >= 0; rank--)
        {
            if (rank < 7)
            {
                sb.Append('/');
            }

            int empty = 0;
            for (int file = 0; file < 8; file++)
            {
                var sq = new Square(8 * rank + file);
                ulong bit = sq.Bit;
                char piece;
                if ((board.Black & bit) != 0)
                {
                    piece = 'p';
                }
                else if ((board.White & bit) != 0)
                {
                    piece = 'P';
                }
                else
                {
                    empty++;
                    continue;
                }

                if (empty > 0)
                {
                    sb.Append((char)('0' + empty));
                    empty = 0;
                }

                sb.Append(piece);
            }

            if (empty > 0)
            {
                sb.Append((char)('0' + empty));
            }
        }

        sb.Append(toMove == Player.Black ? " b - - 0 1" : " w - - 0 1");
        return sb.ToString();
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;
        Process? process = _process;
        _process = null;
        if (process is null)
        {
            return;
        }

        try
        {
            if (!process.HasExited)
            {
                try
                {
                    process.StandardInput.WriteLine("quit");
                    process.StandardInput.Flush();
                    if (!process.WaitForExit(1000))
                    {
                        process.Kill(entireProcessTree: true);
                        process.WaitForExit(1000);
                    }
                }
                catch
                {
                    try { process.Kill(entireProcessTree: true); } catch { }
                }
            }
        }
        finally
        {
            process.Dispose();
        }
    }

    private void StartProcess()
    {
        var psi = new ProcessStartInfo
        {
            FileName = _exePath,
            WorkingDirectory = Path.GetDirectoryName(_exePath) ?? Program.RepositoryFolder(),
            UseShellExecute = false,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        psi.ArgumentList.Add("-xboard");
        psi.ArgumentList.Add("-q");
        psi.ArgumentList.Add("-book-usage");
        psi.ArgumentList.Add("off");
        psi.ArgumentList.Add("-ponder");
        psi.ArgumentList.Add("off");
        psi.ArgumentList.Add("-eval-file");
        psi.ArgumentList.Add(_evalPath);
        psi.ArgumentList.Add("-h");
        psi.ArgumentList.Add(_hashBits.ToString(CultureInfo.InvariantCulture));
        psi.ArgumentList.Add("-n");
        psi.ArgumentList.Add(_threads.ToString(CultureInfo.InvariantCulture));

        var process = new Process { StartInfo = psi };
        process.ErrorDataReceived += (_, e) =>
        {
            if (e.Data is not null)
            {
                lock (_stderrLock)
                {
                    if (_stderrTail.Count >= 20)
                    {
                        _stderrTail.RemoveAt(0);
                    }

                    _stderrTail.Add(e.Data);
                }
            }
        };

        if (!process.Start())
        {
            process.Dispose();
            throw new InvalidOperationException($"Failed to start Edax process: {_exePath}");
        }

        process.BeginErrorReadLine();
        _process = process;

        SendLine(process, "xboard");
        SendLine(process, "protover 2");
        SendLine(process, "easy");
        SendLine(process, "post");
        SendLine(process, "force");
        SendLine(process, "ping 1");
        process.StandardInput.Flush();

        while (true)
        {
            string? line = process.StandardOutput.ReadLine();
            if (line is null)
            {
                throw new InvalidOperationException($"Edax exited during xboard handshake.{StderrSummary()}");
            }

            if (line.Trim() == "pong 1")
            {
                break;
            }
        }
    }

    private void EnsureRunning()
    {
        if (_process is null || _process.HasExited)
        {
            _process?.Dispose();
            _process = null;
            _synced = false;
            _sentLevel = false;
            StartProcess();
        }
    }

    private void SyncBoard(Process process, Board board, Player player)
    {
        if (_synced)
        {
            var commands = new List<string>(4);
            if (TryFindSyncCommands(_edaxBoard, _edaxPlayer, board, player, commands, maxPlies: 4))
            {
                foreach (string cmd in commands)
                {
                    SendLine(process, cmd);
                }

                return;
            }
        }

        SendLine(process, "force");
        SendLine(process, $"setboard {ToFen(board, player)}");
    }

    private void SendLimits(Process process, SearchLimits limits)
    {
        switch (limits.Mode)
        {
            case TimeControlMode.TimePerMove:
                SendLine(process, "sd 60");
                double sec = Math.Max(0.01, limits.Time.TotalSeconds);
                SendLine(process, string.Create(CultureInfo.InvariantCulture, $"st {sec:0.###}"));
                break;
            case TimeControlMode.TimePerGame:
                SendLine(process, "sd 60");
                if (!_sentLevel)
                {
                    int totalSeconds = Math.Max(1, (int)Math.Ceiling(limits.Time.TotalSeconds));
                    SendLine(process, string.Create(CultureInfo.InvariantCulture, $"level 0 {totalSeconds / 60}:{totalSeconds % 60:00} 0"));
                    _sentLevel = true;
                }

                int cs = Math.Max(1, (int)Math.Round(limits.Time.TotalMilliseconds / 10.0));
                SendLine(process, string.Create(CultureInfo.InvariantCulture, $"time {cs}"));
                SendLine(process, string.Create(CultureInfo.InvariantCulture, $"otim {cs}"));
                break;
            case TimeControlMode.FixedDepth:
                SendLine(process, "st 86400");
                SendLine(process, string.Create(CultureInfo.InvariantCulture, $"sd {limits.Depth}"));
                break;
            default:
                SendLine(process, "st 86400");
                SendLine(process, "sd 60");
                break;
        }
    }

    private static bool TryFindSyncCommands(Board current, Player currentPlayer, Board target, Player targetPlayer, List<string> commands, int maxPlies)
    {
        if (current == target && currentPlayer == targetPlayer)
        {
            return true;
        }

        if (maxPlies <= 0)
        {
            return false;
        }

        ulong currentAll = current.Black | current.White;
        ulong targetAll = target.Black | target.White;
        if ((targetAll & currentAll) != currentAll)
        {
            return false;
        }

        if (!current.HasLegalMove(currentPlayer))
        {
            if (!current.HasLegalMove(currentPlayer.Opponent()))
            {
                return false;
            }

            commands.Add("usermove @@@@");
            if (TryFindSyncCommands(current, currentPlayer.Opponent(), target, targetPlayer, commands, maxPlies - 1))
            {
                return true;
            }

            commands.RemoveAt(commands.Count - 1);
            return false;
        }

        ulong added = targetAll & ~currentAll;
        ulong candidates = current.LegalMoves(currentPlayer) & added;
        foreach (Square sq in Square.InMask(candidates))
        {
            Board next = current.Play(currentPlayer, sq);
            commands.Add($"usermove {sq}");
            if (TryFindSyncCommands(next, currentPlayer.Opponent(), target, targetPlayer, commands, maxPlies - 1))
            {
                return true;
            }

            commands.RemoveAt(commands.Count - 1);
        }

        return false;
    }

    private static bool TryParseObserverLine(string line, out int depth, out int score, out long nodes, out bool hasSelectivity)
    {
        depth = 0;
        score = 0;
        nodes = 0;
        hasSelectivity = false;

        string[] parts = line.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length < 4 ||
            !int.TryParse(parts[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out depth) ||
            !int.TryParse(parts[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out int scoreX100) ||
            !long.TryParse(parts[2], NumberStyles.Integer, CultureInfo.InvariantCulture, out _) ||
            !long.TryParse(parts[3], NumberStyles.Integer, CultureInfo.InvariantCulture, out nodes))
        {
            return false;
        }

        score = scoreX100 / 100;
        hasSelectivity = parts.Length >= 5 && parts[4].StartsWith('@');
        return true;
    }

    private static void SendLine(Process process, string line) => process.StandardInput.WriteLine(line);

    private void KillQuietly()
    {
        try
        {
            if (_process is { HasExited: false } p)
            {
                p.Kill(entireProcessTree: true);
            }
        }
        catch
        {
        }
    }

    private string StderrSummary()
    {
        lock (_stderrLock)
        {
            return _stderrTail.Count == 0 ? "" : $" Stderr: {string.Join(" | ", _stderrTail)}";
        }
    }
}
