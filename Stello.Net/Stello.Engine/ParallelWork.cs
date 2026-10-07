using System.Runtime.ExceptionServices;

namespace Stello.Engine;

internal static class ParallelWork
{
    /// <summary>Runs <paramref name="work"/> on <paramref name="workers"/> threads and waits for all of them.</summary>
    /// <exception cref="OperationCanceledException"><paramref name="cancellationToken"/> was cancelled.</exception>
    public static void Run(int workers, Action work, CancellationToken cancellationToken)
    {
        Task[] tasks = Enumerable.Range(0, workers)
            .Select(_ => Task.Factory.StartNew(work, cancellationToken, TaskCreationOptions.LongRunning, TaskScheduler.Default))
            .ToArray();
        try
        {
            Task.WaitAll(tasks);
        }
        catch (AggregateException) when (cancellationToken.IsCancellationRequested)
        {
            throw new OperationCanceledException(cancellationToken);
        }
        catch (AggregateException exception)
        {
            ExceptionDispatchInfo.Throw(exception.InnerExceptions[0]);
        }
    }
}
