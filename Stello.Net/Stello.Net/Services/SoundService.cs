using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Media;
using System.Threading.Channels;
using Stello.App.Models;
using Stello.App.Services;

namespace Stello.Net.Services;

/// <summary>Plays generated sounds in order off the UI thread.</summary>
internal sealed class SoundService : ISoundService, IDisposable
{
    private readonly Channel<(SoundType Sound, int Generation)> _queue = Channel.CreateUnbounded<(SoundType, int)>(
        new UnboundedChannelOptions { SingleReader = true });
    private readonly Action<byte[]> _playWave;
    private readonly Action<Exception> _reportError;
    private bool _enabled = true;
    private bool _disposed;
    private int _generation;

    public SoundService(Action<Exception> reportError)
        : this(PlayWave, reportError)
    {
    }

    internal SoundService(Action<byte[]> playWave, Action<Exception> reportError)
    {
        _playWave = playWave;
        _reportError = reportError;
        Completion = Task.Run(PlayQueueAsync);
    }

    internal Task Completion { get; }

    public bool IsEnabled
    {
        get => Volatile.Read(ref _enabled);
        set
        {
            Volatile.Write(ref _enabled, value);
            if (!value)
            {
                Interlocked.Increment(ref _generation);
            }
        }
    }

    public void Play(SoundType sound)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (!Enum.IsDefined(sound))
        {
            throw new ArgumentOutOfRangeException(nameof(sound));
        }
        if (IsEnabled && !_queue.Writer.TryWrite((sound, Volatile.Read(ref _generation))))
        {
            throw new InvalidOperationException("The sound playback queue is closed.");
        }
    }

    private async Task PlayQueueAsync()
    {
        await foreach (var request in _queue.Reader.ReadAllAsync())
        {
            if (!IsEnabled || request.Generation != Volatile.Read(ref _generation))
            {
                continue;
            }
            try
            {
                _playWave(SoundWaves.Create(request.Sound));
            }
            catch (Exception exception) when (exception is InvalidOperationException or IOException or TimeoutException or Win32Exception)
            {
                Trace.TraceError($"Sound playback failed: {exception}");
                _reportError(exception);
            }
        }
    }

    private static void PlayWave(byte[] wave)
    {
        using var stream = new MemoryStream(wave, writable: false);
        using var player = new SoundPlayer(stream);
        player.PlaySync();
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }
        _disposed = true;
        IsEnabled = false;
        _queue.Writer.TryComplete();
    }
}
