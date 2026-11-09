// SPDX-License-Identifier: GPL-3.0-only

using System.Runtime.InteropServices;
using System.Security.Cryptography;
using Microsoft.Win32.SafeHandles;
using Stello.Engine;
using Stello.Engine.Native;

namespace Stello.Benchmark;

/// <summary>A separately loaded Stello ABI 3 library, identified by its file hash.</summary>
internal sealed class StelloLibrary : IDisposable
{
    private readonly ModuleHandle _module;
    private readonly CreateEngineDelegate _createEngine;
    private readonly DestroyDelegate _destroyEngine;
    private readonly EngineStatusDelegate _clearHash;
    private readonly SetThreadsDelegate _setThreads;
    private readonly SearchDelegate _search;
    private readonly CreateSignalsDelegate _createSignals;
    private readonly DestroyDelegate _destroySignals;
    private readonly SignalsDelegate _resetSignals;
    private readonly SignalsDelegate _cancelSignals;

    private StelloLibrary(ModuleHandle module, string path, string sha256)
    {
        _module = module;
        Path = path;
        Sha256 = sha256;
        Version = Export<VersionDelegate>("stello_version")();
        uint abi = Export<VersionDelegate>("stello_abi_version")();
        if (abi != StelloNative.AbiVersion)
        {
            throw new ArgumentException($"The Stello library has ABI {abi}; this match tool needs ABI {StelloNative.AbiVersion}.");
        }

        _createEngine = Export<CreateEngineDelegate>("stello_engine_create");
        _destroyEngine = Export<DestroyDelegate>("stello_engine_destroy");
        _clearHash = Export<EngineStatusDelegate>("stello_engine_clear_hash");
        _setThreads = Export<SetThreadsDelegate>("stello_engine_set_threads");
        _search = Export<SearchDelegate>("stello_engine_search");
        _createSignals = Export<CreateSignalsDelegate>("stello_signals_create");
        _destroySignals = Export<DestroyDelegate>("stello_signals_destroy");
        _resetSignals = Export<SignalsDelegate>("stello_signals_reset");
        _cancelSignals = Export<SignalsDelegate>("stello_signals_cancel");
    }

    public string Path { get; }

    public string Sha256 { get; }

    public uint Version { get; }

    public string VersionText => $"{Version / 10_000}.{Version / 100 % 100}.{Version % 100}";

    /// <summary>Locates the preserved Stello 3.1 Rust native library (<c>stello_engine_v3</c>), verifies its version and computes its SHA-256.</summary>
    public static (string Path, string Sha256, string VersionText) ResolveV3()
    {
        string os = OperatingSystem.IsWindows() ? "windows" : OperatingSystem.IsMacOS() ? "macos" : "linux";
        string file = OperatingSystem.IsWindows()
            ? "stello_engine_v3.dll"
            : OperatingSystem.IsMacOS() ? "libstello_engine_v3.dylib" : "libstello_engine_v3.so";

        string? envPath = Environment.GetEnvironmentVariable("STELLO_V3_LIBRARY");
        string? found = null;
        if (!string.IsNullOrWhiteSpace(envPath))
        {
            string fullEnv = System.IO.Path.GetFullPath(envPath);
            found = File.Exists(fullEnv)
                ? fullEnv
                : throw new ArgumentException($"STELLO_V3_LIBRARY points to a missing file: {fullEnv}");
        }
        else
        {
            string repo = Program.RepositoryFolder();
            string[] candidates =
            [
                System.IO.Path.Combine(AppContext.BaseDirectory, file),
                System.IO.Path.Combine(repo, "Stello.Rust", "target", $"{os}-v3", "release", file),
                System.IO.Path.Combine(repo, "Stello.Rust", "target", os, "release", file),
            ];
            found = candidates.FirstOrDefault(File.Exists)
                ?? throw new ArgumentException(
                    $"Could not find {file} for engine=stello-v3. Build Stello.Rust/stello-ffi with Cargo into " +
                    $"Stello.Rust/target/{os}-v3/release/{file} (or set STELLO_V3_LIBRARY).");
        }

        string hash;
        using (var stream = new FileStream(found, FileMode.Open, FileAccess.Read, FileShare.Read))
        {
            hash = Convert.ToHexString(SHA256.HashData(stream));
        }

        using StelloLibrary library = Open(found, hash);
        if (library.Version is < 30_100 or >= 30_200)
        {
            throw new ArgumentException($"Expected Stello 3.1 (301xx) at {found}, found {library.VersionText} ({library.Version}).");
        }

        return (library.Path, library.Sha256, library.VersionText);
    }

    public static StelloLibrary Open(string path, string expectedSha256)
    {
        if (string.IsNullOrWhiteSpace(path) || !System.IO.Path.IsPathFullyQualified(path))
        {
            throw new ArgumentException("lib must be an absolute path to the Stello library.");
        }

        if (expectedSha256.Length != 64 || !expectedSha256.All(char.IsAsciiHexDigit))
        {
            throw new ArgumentException("sha256 must be the library's 64 hexadecimal SHA-256 digits.");
        }

        string fullPath = System.IO.Path.GetFullPath(path);
        string fileName = System.IO.Path.GetFileName(fullPath);
        if (new[] { "stello_engine.dll", "libstello_engine.so", "libstello_engine.dylib" }
            .Contains(fileName, StringComparer.OrdinalIgnoreCase))
        {
            throw new ArgumentException("lib must have a different file name from the current Stello engine, so the two libraries cannot be confused.");
        }

        try
        {
            using var stream = new FileStream(fullPath, FileMode.Open, FileAccess.Read, FileShare.Read);
            string actualSha256 = Convert.ToHexString(SHA256.HashData(stream));
            if (!string.Equals(actualSha256, expectedSha256, StringComparison.OrdinalIgnoreCase))
            {
                throw new ArgumentException($"The SHA-256 of {fullPath} is {actualSha256}, not the requested {expectedSha256}.");
            }

            try
            {
                StelloNative.EnsureLoaded();
            }
            catch (Exception exception) when (exception is DllNotFoundException or InvalidOperationException)
            {
                throw new ArgumentException($"Cannot load the current Stello engine: {exception.Message}", exception);
            }

            nint rawModule;
            try
            {
                rawModule = NativeLibrary.Load(fullPath);
            }
            catch (Exception exception) when (exception is DllNotFoundException or BadImageFormatException)
            {
                throw new ArgumentException($"Cannot load Stello library {fullPath}: {exception.Message}", exception);
            }

            var module = new ModuleHandle(rawModule);
            bool opened = false;
            try
            {
                StelloLibrary library;
                try
                {
                    library = new StelloLibrary(module, fullPath, actualSha256);
                }
                catch (EntryPointNotFoundException exception)
                {
                    throw new ArgumentException($"Stello library {fullPath} is missing a required ABI export: {exception.Message}", exception);
                }

                opened = true;
                return library;
            }
            finally
            {
                if (!opened)
                {
                    module.Dispose();
                }
            }
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            throw new ArgumentException($"Cannot read Stello library {fullPath}: {exception.Message}", exception);
        }
    }

    public EngineHandle CreateEngine(int hashBits)
    {
        nint pointer = _createEngine(hashBits, 1, -1, -1, 0, 0, out int status);
        if (pointer == 0)
        {
            throw new InvalidOperationException($"The Stello library could not create an engine (status {status}).");
        }

        bool owned = false;
        try
        {
            Check(status);
            var engine = new EngineHandle(this, pointer, hashBits);
            owned = true;
            return engine;
        }
        finally
        {
            if (!owned)
            {
                _destroyEngine(pointer);
            }
        }
    }

    public SignalsHandle CreateSignals()
    {
        nint pointer = _createSignals();
        if (pointer == 0)
        {
            throw new InvalidOperationException("The Stello library could not create search signals.");
        }

        bool owned = false;
        try
        {
            var signals = new SignalsHandle(this, pointer);
            owned = true;
            return signals;
        }
        finally
        {
            if (!owned)
            {
                _destroySignals(pointer);
            }
        }
    }

    public void ClearHash(EngineHandle engine) => Check(_clearHash(engine));

    public void SetThreads(EngineHandle engine, int threads) => Check(_setThreads(engine, threads));

    public void ResetSignals(SignalsHandle signals) => _resetSignals(signals);

    public int Search(EngineHandle engine, SignalsHandle signals, Board board, Player player, SearchLimits limits, out NativeSearchResult result) =>
        _search(engine, board.Black, board.White, (int)player, (int)limits.Mode, limits.Depth,
            (long)limits.Time.TotalMilliseconds, 0, 0, signals, 0, 0, out result);

    public void Dispose() => _module.Dispose();

    public static void Check(int status)
    {
        if (status == StelloNative.InvalidArgument)
        {
            throw new ArgumentException("The Stello library rejected the search arguments.");
        }

        if (status != StelloNative.Ok)
        {
            throw new InvalidOperationException($"The Stello library failed (status {status}).");
        }
    }

    private T Export<T>(string name) where T : Delegate =>
        Marshal.GetDelegateForFunctionPointer<T>(NativeLibrary.GetExport(_module.DangerousGetHandle(), name));

    private sealed class ModuleHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        public ModuleHandle(nint pointer)
            : base(ownsHandle: true) => SetHandle(pointer);

        protected override bool ReleaseHandle()
        {
            NativeLibrary.Free(handle);
            return true;
        }
    }

    internal sealed class EngineHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        private readonly StelloLibrary _owner;
        private readonly long _memoryPressure;

        public EngineHandle(StelloLibrary owner, nint pointer, int hashBits)
            : base(ownsHandle: true)
        {
            _owner = owner;
            bool added = false;
            owner._module.DangerousAddRef(ref added);
            SetHandle(pointer);
            _memoryPressure = 128L << hashBits;
            bool pressureAdded = false;
            try
            {
                GC.AddMemoryPressure(_memoryPressure);
                pressureAdded = true;
            }
            finally
            {
                if (!pressureAdded)
                {
                    SetHandleAsInvalid();
                    owner._module.DangerousRelease();
                }
            }
        }

        protected override bool ReleaseHandle()
        {
            try
            {
                _owner._destroyEngine(handle);
                GC.RemoveMemoryPressure(_memoryPressure);
                return true;
            }
            finally
            {
                _owner._module.DangerousRelease();
            }
        }
    }

    internal sealed class SignalsHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        private readonly StelloLibrary _owner;

        public SignalsHandle(StelloLibrary owner, nint pointer)
            : base(ownsHandle: true)
        {
            _owner = owner;
            bool added = false;
            owner._module.DangerousAddRef(ref added);
            SetHandle(pointer);
        }

        public void Cancel() => _owner._cancelSignals(this);

        protected override bool ReleaseHandle()
        {
            try
            {
                _owner._destroySignals(handle);
                return true;
            }
            finally
            {
                _owner._module.DangerousRelease();
            }
        }
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate uint VersionDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate nint CreateEngineDelegate(int bits, int selective, int probCut, int softTime, nint eval, nuint evalLength, out int status);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void DestroyDelegate(nint pointer);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int EngineStatusDelegate(EngineHandle engine);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int SetThreadsDelegate(EngineHandle engine, int threads);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int SearchDelegate(EngineHandle engine, ulong black, ulong white, int player, int mode, int depth, long timeMs,
        ulong onlyMoves, int hasOnlyMoves, SignalsHandle signals, nint progress, nint progressUser, out NativeSearchResult result);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate nint CreateSignalsDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void SignalsDelegate(SignalsHandle signals);
}

/// <summary>A no-book match player using a separately loaded Stello library.</summary>
internal sealed class StelloLibraryMatchPlayer(string path, string sha256, int hashBits) : IMatchPlayer, IDisposable
{
    private readonly StelloLibrary _library = StelloLibrary.Open(path, sha256);
    private StelloLibrary.EngineHandle? _engine;
    private StelloLibrary.SignalsHandle? _signals;
    private int _threads = 1;
    private bool _started;
    private bool _disposed;

    public void NewGame()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (_engine is not null)
        {
            _library.ClearHash(_engine);
        }

        _started = true;
    }

    public SearchResult ChooseMove(Board board, Player player, SearchLimits limits, CancellationToken cancellationToken)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (!_started)
        {
            throw new InvalidOperationException("Call NewGame before the first move.");
        }

        cancellationToken.ThrowIfCancellationRequested();
        (StelloLibrary.EngineHandle engine, StelloLibrary.SignalsHandle signals) = Native();
        _library.ResetSignals(signals);
        if (limits.Threads != _threads)
        {
            _library.SetThreads(engine, limits.Threads);
            _threads = limits.Threads;
        }

        using CancellationTokenRegistration registration =
            cancellationToken.UnsafeRegister(static state => ((StelloLibrary.SignalsHandle)state!).Cancel(), signals);
        int status = _library.Search(engine, signals, board, player, limits, out NativeSearchResult result);
        if (status == StelloNative.Cancelled)
        {
            throw new OperationCanceledException(cancellationToken);
        }

        StelloLibrary.Check(status);
        return new SearchResult(
            result.Move >= 0 ? new Move(new Square(result.Move)) : Move.Pass,
            result.Score,
            (ScoreKind)result.Kind,
            result.Depth,
            (long)result.Nodes,
            (long)result.Evaluations,
            StelloNative.Elapsed(result.ElapsedNs));
    }

    public void Dispose()
    {
        if (!_disposed)
        {
            _signals?.Dispose();
            _engine?.Dispose();
            _library.Dispose();
            _disposed = true;
        }
    }

    private (StelloLibrary.EngineHandle Engine, StelloLibrary.SignalsHandle Signals) Native()
    {
        if (_engine is not null && _signals is not null)
        {
            return (_engine, _signals);
        }

        StelloLibrary.EngineHandle engine = _library.CreateEngine(hashBits);
        try
        {
            _signals = _library.CreateSignals();
            _engine = engine;
            return (engine, _signals);
        }
        finally
        {
            if (_engine is null)
            {
                engine.Dispose();
            }
        }
    }
}
