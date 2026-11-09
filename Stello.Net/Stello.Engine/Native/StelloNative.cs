using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace Stello.Engine.Native;

/// <summary>
/// The C ABI of the C++ engine, <c>stello_engine</c> (<c>Stello.Cpp/src/ffi/stello_ffi.hpp</c> and <c>stello_ffi.cpp</c>,
/// ported from <c>Stello.Rust/stello-ffi/src/lib.rs</c>): <c>libstello_engine.so</c> on Linux and
/// <c>stello_engine.dll</c> on Windows, next to the application. Stello.Engine's build compiles it with CMake + Clang
/// and copies it to every output that references Stello.Engine.
/// </summary>
internal static unsafe partial class StelloNative
{
    private const string Library = "stello_engine";

    /// <summary>The ABI version this wrapper is written for (<c>kAbiVersion</c> in Stello.Cpp).</summary>
    public const uint AbiVersion = 3;

    // Status codes (STELLO_OK ... in Stello.Cpp/src/ffi/stello_ffi.hpp).
    public const int Ok = 0;
    public const int Cancelled = 1;
    public const int NoLegalOnlyMove = 2;
    public const int InvalidArgument = 3;
    public const int InvalidEvaluation = 4;
    public const int InternalError = 5;

    [LibraryImport(Library, EntryPoint = "stello_version")]
    public static partial uint Version();

    [LibraryImport(Library, EntryPoint = "stello_abi_version")]
    public static partial uint LibraryAbiVersion();

    [LibraryImport(Library, EntryPoint = "stello_engine_create")]
    public static partial EngineHandle CreateEngine(
        int hashBits, int selective, int probCutPercent, int softTimePercent, byte* eval, nuint evalLength, out int status);

    [LibraryImport(Library, EntryPoint = "stello_engine_destroy")]
    public static partial void DestroyEngine(nint engine);

    [LibraryImport(Library, EntryPoint = "stello_engine_clear_hash")]
    public static partial int ClearHash(EngineHandle engine);

    [LibraryImport(Library, EntryPoint = "stello_engine_set_threads")]
    public static partial int SetThreads(EngineHandle engine, int threads);

    [LibraryImport(Library, EntryPoint = "stello_engine_set_ybwc")]
    public static partial int SetYbwc(EngineHandle engine, int ybwc);

    [LibraryImport(Library, EntryPoint = "stello_engine_set_iteration_done")]
    public static partial int SetIterationDone(EngineHandle engine, delegate* unmanaged[Cdecl]<int, int, nint, void> callback, nint user);

    [LibraryImport(Library, EntryPoint = "stello_engine_search")]
    public static partial int Search(
        EngineHandle engine,
        ulong black,
        ulong white,
        int player,
        int mode,
        int depth,
        long timeMs,
        ulong onlyMoves,
        int hasOnlyMoves,
        SignalsHandle signals,
        delegate* unmanaged[Cdecl]<NativeSearchInfo*, nint, void> progress,
        nint progressUser,
        out NativeSearchResult result);

    [LibraryImport(Library, EntryPoint = "stello_signals_create")]
    public static partial SignalsHandle CreateSignals();

    [LibraryImport(Library, EntryPoint = "stello_signals_destroy")]
    public static partial void DestroySignals(nint signals);

    [LibraryImport(Library, EntryPoint = "stello_signals_cancel")]
    public static partial void Cancel(SignalsHandle signals);

    [LibraryImport(Library, EntryPoint = "stello_signals_move_now")]
    public static partial void MoveNow(SignalsHandle signals);

    [LibraryImport(Library, EntryPoint = "stello_signals_reset")]
    public static partial void ResetSignals(SignalsHandle signals);

    /// <summary>The engine's elapsed time in nanoseconds as a <see cref="TimeSpan"/> (100 ns ticks).</summary>
    public static TimeSpan Elapsed(ulong nanoseconds) => TimeSpan.FromTicks((long)(nanoseconds / 100));

    private static readonly Lazy<Exception?> LoadError = new(() =>
    {
        try
        {
            uint abi = LibraryAbiVersion();
            return abi == AbiVersion
                ? null
                : new InvalidOperationException(
                    $"The C++ engine library {Library} has ABI version {abi}, but this Stello.Engine needs {AbiVersion}. " +
                    "Rebuild Stello.Engine, which builds Stello.Cpp with CMake and Clang.");
        }
        catch (Exception exception) when (exception is DllNotFoundException or EntryPointNotFoundException or BadImageFormatException)
        {
            return new DllNotFoundException(
                $"The C++ engine library {Library} ({(OperatingSystem.IsWindows() ? "stello_engine.dll" : "libstello_engine.so")}) " +
                $"could not be loaded from {AppContext.BaseDirectory}: {exception.Message} Stello.Engine's build makes it " +
                "with CMake + Clang (Stello.Cpp) and copies it next to the application.", exception);
        }
    });

    /// <summary>Checks once that the library loads and has the right ABI version.</summary>
    /// <exception cref="DllNotFoundException">The library is missing or cannot be loaded.</exception>
    /// <exception cref="InvalidOperationException">The library has another ABI version.</exception>
    public static void EnsureLoaded()
    {
        if (LoadError.Value is { } error)
        {
            throw error;
        }
    }
}

/// <summary>An engine instance of the native library (<c>stello_engine_create</c>), with the memory of its hash tables.</summary>
internal sealed class EngineHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    private long _memoryPressure;

    public EngineHandle()
        : base(ownsHandle: true)
    {
    }

    /// <summary>Tells the garbage collector about the native memory, so engines that are no longer used are freed.</summary>
    public void AddMemoryPressure(long bytes)
    {
        GC.AddMemoryPressure(bytes);
        _memoryPressure = bytes;
    }

    protected override bool ReleaseHandle()
    {
        StelloNative.DestroyEngine(handle);
        if (_memoryPressure > 0)
        {
            GC.RemoveMemoryPressure(_memoryPressure);
        }

        return true;
    }
}

/// <summary>Stop signals of the native library (<c>stello_signals_create</c>): cancel and Move Now.</summary>
internal sealed class SignalsHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    public SignalsHandle()
        : base(ownsHandle: true)
    {
    }

    protected override bool ReleaseHandle()
    {
        StelloNative.DestroySignals(handle);
        return true;
    }
}

/// <summary>Native <c>StelloSearchInfo</c> (48 bytes, the same field order).</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeSearchInfo
{
    public ulong Nodes;
    public ulong Evaluations;
    public ulong ElapsedNs;
    public int Depth;
    public int CurrentMove;
    public int BestMove;
    public int Score;
    public int Kind;
    public int Reserved;
}

/// <summary>Native <c>StelloSearchResult</c> (40 bytes, the same field order).</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeSearchResult
{
    public ulong Nodes;
    public ulong Evaluations;
    public ulong ElapsedNs;
    public int Move;
    public int Score;
    public int Kind;
    public int Depth;
}
