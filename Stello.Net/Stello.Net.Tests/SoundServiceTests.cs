using Stello.App.Models;
using Stello.App.Services;
using Stello.Net.Services;

namespace Stello.Net.Tests;

public sealed class SoundServiceTests
{
    [Fact]
    public async Task Sounds_PlayGeneratedWavesInOrderAndRespectMute()
    {
        var played = new List<byte[]>();
        var finished = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        using var service = new SoundService(wave =>
        {
            played.Add(wave);
            if (played.Count == 3)
            {
                finished.SetResult();
            }
        }, exception => finished.TrySetException(exception));
        service.IsEnabled = false;
        service.Play(SoundType.Loss);
        service.IsEnabled = true;
        service.Play(SoundType.Move);
        service.Play(SoundType.Pass);
        service.Play(SoundType.Win);
        await finished.Task.WaitAsync(TimeSpan.FromSeconds(5));
        service.Dispose();
        await service.Completion.WaitAsync(TimeSpan.FromSeconds(5));
        Assert.Equal([SoundWaves.Create(SoundType.Move), SoundWaves.Create(SoundType.Pass), SoundWaves.Create(SoundType.Win)], played);
    }

    [Fact]
    public async Task Mute_DiscardsQueuedSoundsEvenAfterReenabling()
    {
        using var started = new ManualResetEventSlim();
        using var release = new ManualResetEventSlim();
        var played = new List<byte[]>();
        var finished = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        using var service = new SoundService(wave =>
        {
            played.Add(wave);
            if (played.Count == 1)
            {
                started.Set();
                Assert.True(release.Wait(TimeSpan.FromSeconds(5)));
            }
            else
            {
                finished.SetResult();
            }
        }, exception => throw new InvalidOperationException("Unexpected playback error.", exception));
        service.Play(SoundType.Move);
        Assert.True(started.Wait(TimeSpan.FromSeconds(5)));
        service.Play(SoundType.Win);
        service.IsEnabled = false;
        service.IsEnabled = true;
        service.Play(SoundType.Pass);
        release.Set();
        await finished.Task.WaitAsync(TimeSpan.FromSeconds(5));
        service.Dispose();
        await service.Completion.WaitAsync(TimeSpan.FromSeconds(5));
        Assert.Equal([SoundWaves.Create(SoundType.Move), SoundWaves.Create(SoundType.Pass)], played);
    }

    [Fact]
    public async Task PlaybackFailure_IsReportedAndTheQueueCanFinish()
    {
        var reported = new TaskCompletionSource<Exception>(TaskCreationOptions.RunContinuationsAsynchronously);
        using var service = new SoundService(_ => throw new InvalidOperationException("No sound device."),
            exception => reported.TrySetResult(exception));
        service.Play(SoundType.Move);
        Assert.Equal("No sound device.", (await reported.Task.WaitAsync(TimeSpan.FromSeconds(5))).Message);
        service.Dispose();
        await service.Completion.WaitAsync(TimeSpan.FromSeconds(5));
        Assert.Throws<ObjectDisposedException>(() => service.Play(SoundType.Move));
    }
}
