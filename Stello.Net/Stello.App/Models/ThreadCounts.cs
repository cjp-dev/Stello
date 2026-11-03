using Stello.Engine;

namespace Stello.App.Models;

/// <summary>
/// How many threads the computer may search with on this device: <see cref="Device"/> is the most the Threads setting
/// offers, <see cref="Default"/> what its Automatic choice uses (Plans/making-stello-3-plan.md, phase 9).
/// </summary>
public sealed record ThreadCounts(int Device, int Default)
{
    /// <summary>The desktop: all the processor's threads offered, all but one by default (the app keeps one).</summary>
    public static ThreadCounts ForThisProcess()
    {
        int count = Math.Clamp(Environment.ProcessorCount, 1, SearchLimits.MaxThreads);
        return new ThreadCounts(count, Math.Max(1, count - 1));
    }

    /// <summary>The browser: <c>navigator.hardwareConcurrency</c>, offered and used by default (the page's own thread is
    /// not one of the engine's).</summary>
    public static ThreadCounts ForBrowser(int hardwareConcurrency)
    {
        int count = Math.Clamp(hardwareConcurrency, 1, SearchLimits.MaxThreads);
        return new ThreadCounts(count, count);
    }
}
