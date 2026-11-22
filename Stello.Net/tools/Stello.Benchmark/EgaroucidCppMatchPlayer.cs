// SPDX-License-Identifier: GPL-3.0-only

using System.Diagnostics;
using System.Globalization;
using Stello.Egaroucid;
using Stello.Engine;
using Board = Stello.Engine.Board;
using EgaroucidBoard = Stello.Egaroucid.Board;
using SearchResult = Stello.Engine.SearchResult;

namespace Stello.Benchmark;

/// <summary>
/// Native C++ Egaroucid (<c>Egaroucid-main</c> via <c>Egaroucid.Runner</c>) as a persistent per-worker subprocess
/// <see cref="IMatchPlayer"/>.
/// </summary>
internal sealed class EgaroucidCppMatchPlayer : IMatchPlayer, IDisposable
{
    public const int DefaultHashBits = MatchCommands.EgaroucidDefaultTableBits;

    private readonly int _hashBits;
    private readonly string _runnerPath;
    private readonly string _resourceDir;
    private readonly List<string> _stderrTail = [];
    private readonly Lock _stderrLock = new();

    private Process? _process;
    private int _threads;
    private bool _disposed;

    public EgaroucidCppMatchPlayer(int hashBits = DefaultHashBits, int threads = 1)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(hashBits, 10);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(hashBits, 27);
        ArgumentOutOfRangeException.ThrowIfLessThan(threads, 1);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(threads, SearchLimits.MaxThreads);

        _hashBits = hashBits;
        _threads = threads;
        _runnerPath = ResolveRunnerExecutable();
        _resourceDir = EgaroucidResources.FindDirectory() ?? throw new ArgumentException(
            $"Egaroucid's resources were not found. Put Egaroucid in <repo>/Egaroucid-main or set {EgaroucidResources.EnvironmentVariable}.");
        StartProcess();
    }

    public static string? FindRunnerExecutable()
    {
        string? fromEnv = Environment.GetEnvironmentVariable("EGAROUCID_RUNNER");
        if (!string.IsNullOrWhiteSpace(fromEnv))
        {
            string full = Path.GetFullPath(fromEnv);
            return File.Exists(full) ? full : null;
        }

        string file = OperatingSystem.IsWindows() ? "egaroucid-runner.exe" : "egaroucid-runner";
        string candidate = Path.Combine(Program.RepositoryFolder(), "Stello.Net", "tools", "Egaroucid.Runner", "bin", file);
        return File.Exists(candidate) ? candidate : null;
    }

    public static string ResolveRunnerExecutable() =>
        FindRunnerExecutable() ?? throw new ArgumentException(
            "Could not find egaroucid-runner executable for engine=egaroucid-cpp. Run " +
            "Stello.Net/tools/Egaroucid.Runner/build.cmd (or set EGAROUCID_RUNNER).");

    public void NewGame()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        EnsureRunning();
        SendCommand(_process!, "clear", "ok");
    }

    public SearchResult ChooseMove(Board board, Player player, SearchLimits limits, CancellationToken cancellationToken)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        cancellationToken.ThrowIfCancellationRequested();
        if (limits.Threads != _threads)
        {
            _threads = limits.Threads;
            StopProcess();
        }

        EnsureRunning();
        string mode = limits.Mode switch
        {
            TimeControlMode.TimePerMove => string.Create(CultureInfo.InvariantCulture, $"movetime {Math.Max(1L, (long)Math.Round(limits.Time.TotalMilliseconds))}"),
            TimeControlMode.TimePerGame => string.Create(CultureInfo.InvariantCulture, $"gametime {Math.Max(1L, (long)Math.Round(limits.Time.TotalMilliseconds))}"),
            TimeControlMode.FixedDepth => string.Create(CultureInfo.InvariantCulture, $"level {limits.Depth}"),
            _ => "solve",
        };
        return ExecuteSearch(board, player, mode, cancellationToken);
    }

    public SearchResult SearchDepth(Board board, Player player, int depth, int mpcIndex, CancellationToken cancellationToken = default)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        cancellationToken.ThrowIfCancellationRequested();
        EnsureRunning();
        return ExecuteSearch(board, player, string.Create(CultureInfo.InvariantCulture, $"depth {depth} {mpcIndex}"), cancellationToken);
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;
        StopProcess();
    }

    private SearchResult ExecuteSearch(Board board, Player player, string modeAndArgs, CancellationToken cancellationToken)
    {
        Process process = _process!;
        EgaroucidBoard eb = EgaroucidPlayer.ToEgaroucid(board, player);
        using CancellationTokenRegistration cancelReg = cancellationToken.Register(KillQuietly);
        try
        {
            long started = Stopwatch.GetTimestamp();
            string command = string.Create(CultureInfo.InvariantCulture, $"go {eb.Player:x16} {eb.Opponent:x16} {modeAndArgs}");
            process.StandardInput.WriteLine(command);
            process.StandardInput.Flush();

            string? line = process.StandardOutput.ReadLine();
            TimeSpan elapsed = Stopwatch.GetElapsedTime(started);
            if (line is null)
            {
                cancellationToken.ThrowIfCancellationRequested();
                throw new InvalidOperationException($"egaroucid-runner exited unexpectedly during search.{StderrSummary()}");
            }

            return ParseResult(line, board, player, elapsed);
        }
        catch (Exception) when (cancellationToken.IsCancellationRequested)
        {
            throw new OperationCanceledException(cancellationToken);
        }
    }

    private static SearchResult ParseResult(string line, Board board, Player player, TimeSpan elapsed)
    {
        string[] parts = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length < 14 ||
            parts[0] != "bestmove" || !int.TryParse(parts[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out int policy) ||
            parts[2] != "score" || !int.TryParse(parts[3], NumberStyles.Integer, CultureInfo.InvariantCulture, out int score) ||
            parts[4] != "depth" || !int.TryParse(parts[5], NumberStyles.Integer, CultureInfo.InvariantCulture, out int depth) ||
            parts[6] != "prob" || !int.TryParse(parts[7], NumberStyles.Integer, CultureInfo.InvariantCulture, out int prob) ||
            parts[8] != "end" || !int.TryParse(parts[9], NumberStyles.Integer, CultureInfo.InvariantCulture, out int isEnd) ||
            parts[10] != "nodes" || !long.TryParse(parts[11], NumberStyles.Integer, CultureInfo.InvariantCulture, out long nodes))
        {
            throw new InvalidOperationException($"Unexpected egaroucid-runner response: {line}");
        }

        if (policy is < 0 or >= Common.HW2)
        {
            throw new InvalidOperationException($"egaroucid-runner returned invalid move index {policy}.");
        }

        Square square = EgaroucidPlayer.ToSquare(policy);
        if (!board.IsLegal(player, square))
        {
            throw new InvalidOperationException($"egaroucid-runner returned illegal move {square} (policy {policy}).");
        }

        ScoreKind kind = isEnd != 0 && prob == 100 ? ScoreKind.Exact : ScoreKind.Heuristic;
        return new SearchResult(new Move(square), score, kind, Math.Max(0, depth), nodes, 0, elapsed);
    }

    private void StartProcess()
    {
        var psi = new ProcessStartInfo
        {
            FileName = _runnerPath,
            WorkingDirectory = Path.GetDirectoryName(_runnerPath) ?? Program.RepositoryFolder(),
            UseShellExecute = false,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
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
            throw new InvalidOperationException($"Failed to start egaroucid-runner process: {_runnerPath}");
        }

        process.BeginErrorReadLine();
        _process = process;
        SendCommand(process, string.Create(CultureInfo.InvariantCulture, $"init {_threads} {_hashBits} {_resourceDir}"), "ready");
    }

    private void EnsureRunning()
    {
        if (_process is null || _process.HasExited)
        {
            _process?.Dispose();
            _process = null;
            StartProcess();
        }
    }

    private void StopProcess()
    {
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

    private void SendCommand(Process process, string command, string expected)
    {
        process.StandardInput.WriteLine(command);
        process.StandardInput.Flush();
        string? line = process.StandardOutput.ReadLine();
        if (line is null || line.Trim() != expected)
        {
            throw new InvalidOperationException($"egaroucid-runner failed on \"{command}\" (got \"{line ?? "<eof>"}\").{StderrSummary()}");
        }
    }

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
