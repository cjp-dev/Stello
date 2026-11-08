using System.Buffers.Binary;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Stello.Benchmark;

/// <summary>Physical cores available to the match, rather than the number of logical processors.</summary>
internal static class ProcessorTopology
{
    public static int PhysicalCores()
    {
        try
        {
            int cores = OperatingSystem.IsWindows() ? WindowsCores()
                : OperatingSystem.IsLinux() ? LinuxCores()
                : OperatingSystem.IsMacOS() ? MacCores()
                : throw new InvalidDataException("This operating system is not supported.");
            if (cores < 1)
            {
                throw new InvalidDataException("No available physical cores were found.");
            }

            return Math.Min(cores, Environment.ProcessorCount);
        }
        catch (Exception exception) when (exception is IOException or InvalidDataException or UnauthorizedAccessException
            or Win32Exception or DllNotFoundException or EntryPointNotFoundException)
        {
            throw new ArgumentException($"Cannot determine the physical core count: {exception.Message} Specify --workers explicitly.", exception);
        }
    }

    [SupportedOSPlatform("windows")]
    private static int WindowsCores()
    {
        byte[] topology = WindowsTopology();
        ushort groups = GetActiveProcessorGroupCount();
        if (groups == 0)
        {
            throw new InvalidDataException("Windows reported no active processor groups.");
        }

        // ProcessorAffinity describes one group; on multi-group systems use the system topology.
        ulong? affinity = null;
        if (groups == 1 && Environment.Is64BitProcess)
        {
            using Process process = Process.GetCurrentProcess();
            affinity = unchecked((ulong)process.ProcessorAffinity.ToInt64());
            if (affinity == 0)
            {
                throw new InvalidDataException("Windows reported an empty process affinity.");
            }
        }

        return CountWindowsCores(topology, affinity);
    }

    private static byte[] WindowsTopology()
    {
        const int insufficientBuffer = 122;
        uint size = 0;
        bool success = GetLogicalProcessorInformationEx(0, null, ref size);
        int error = Marshal.GetLastPInvokeError();
        if (success)
        {
            throw new InvalidDataException("Windows returned an unexpected processor topology size.");
        }

        if (error != insufficientBuffer)
        {
            throw new Win32Exception(error);
        }

        for (int attempt = 0; attempt < 3; attempt++)
        {
            if (size == 0 || size > int.MaxValue)
            {
                throw new InvalidDataException("Windows returned an invalid processor topology size.");
            }

            byte[] buffer = new byte[(int)size];
            uint written = size;
            if (GetLogicalProcessorInformationEx(0, buffer, ref written))
            {
                if (written > size)
                {
                    throw new InvalidDataException("Windows returned an invalid processor topology length.");
                }

                return written == size ? buffer : buffer[..(int)written];
            }

            error = Marshal.GetLastPInvokeError();
            if (error != insufficientBuffer)
            {
                throw new Win32Exception(error);
            }

            size = written;
        }

        throw new InvalidDataException("The Windows processor topology changed while it was being read.");
    }

    internal static int CountWindowsCores(ReadOnlySpan<byte> topology, ulong? affinity)
    {
        const int masksOffset = 32;
        int groupSize = IntPtr.Size + 8;
        int cores = 0;
        while (!topology.IsEmpty)
        {
            if (topology.Length < masksOffset)
            {
                throw new InvalidDataException("A Windows processor topology record is truncated.");
            }

            uint relationship = BinaryPrimitives.ReadUInt32LittleEndian(topology);
            uint size = BinaryPrimitives.ReadUInt32LittleEndian(topology[4..]);
            ushort groupCount = BinaryPrimitives.ReadUInt16LittleEndian(topology[30..]);
            if (relationship != 0 || groupCount == 0 || size < masksOffset + groupCount * groupSize || size > topology.Length)
            {
                throw new InvalidDataException("A Windows processor topology record is invalid.");
            }

            bool available = affinity is null;
            for (int groupIndex = 0; groupIndex < groupCount; groupIndex++)
            {
                int offset = masksOffset + groupIndex * groupSize;
                ulong mask = IntPtr.Size == 8
                    ? BinaryPrimitives.ReadUInt64LittleEndian(topology[offset..])
                    : BinaryPrimitives.ReadUInt32LittleEndian(topology[offset..]);
                ushort group = BinaryPrimitives.ReadUInt16LittleEndian(topology[(offset + IntPtr.Size)..]);
                if (affinity is { } allowed && group == 0 && (mask & allowed) != 0)
                {
                    available = true;
                }
            }

            if (available)
            {
                cores++;
            }

            topology = topology[(int)size..];
        }

        return cores;
    }

    private static int LinuxCores()
    {
        string root = Path.DirectorySeparatorChar.ToString();
        string cpuDirectory = Path.Combine(root, "sys", "devices", "system", "cpu");
        string status = Path.Combine(root, "proc", "self", "status");
        const string allowedPrefix = "Cpus_allowed_list:";
        string allowed = File.ReadLines(status).FirstOrDefault(line => line.StartsWith(allowedPrefix, StringComparison.Ordinal))
            ?? throw new InvalidDataException("Linux did not report the process CPU affinity.");

        return CountLinuxCores(
            File.ReadAllText(Path.Combine(cpuDirectory, "online")),
            allowed[allowedPrefix.Length..],
            cpu => File.ReadAllText(Path.Combine(cpuDirectory, "cpu" + cpu.ToString(CultureInfo.InvariantCulture), "topology", "thread_siblings_list")));
    }

    internal static int CountLinuxCores(string onlineList, string allowedList, Func<int, string> readSiblings)
    {
        HashSet<int> online = CpuList(onlineList);
        HashSet<int> allowed = CpuList(allowedList);
        var counted = new HashSet<int>();
        int cores = 0;
        foreach (int cpu in online)
        {
            if (!allowed.Contains(cpu) || counted.Contains(cpu))
            {
                continue;
            }

            HashSet<int> siblings = CpuList(readSiblings(cpu));
            if (!siblings.Contains(cpu))
            {
                throw new InvalidDataException($"Linux processor {cpu} is absent from its sibling list.");
            }

            counted.UnionWith(siblings);
            cores++;
        }

        return cores > 0 ? cores : throw new InvalidDataException("No online Linux processors are available to the process.");
    }

    private static HashSet<int> CpuList(string text)
    {
        var cpus = new HashSet<int>();
        foreach (string part in text.Trim().Split(',', StringSplitOptions.TrimEntries))
        {
            string[] range = part.Split('-', StringSplitOptions.TrimEntries);
            if (range.Length is < 1 or > 2
                || !int.TryParse(range[0], NumberStyles.None, CultureInfo.InvariantCulture, out int first)
                || !int.TryParse(range[^1], NumberStyles.None, CultureInfo.InvariantCulture, out int last)
                || last < first)
            {
                throw new InvalidDataException($"Invalid Linux processor list: {text.Trim()}.");
            }

            for (int cpu = first; ; cpu++)
            {
                cpus.Add(cpu);
                if (cpu == last)
                {
                    break;
                }
            }
        }

        return cpus;
    }

    private static int MacCores()
    {
        nuint size = sizeof(int);
        if (SysctlByName("hw.physicalcpu", out int cores, ref size, IntPtr.Zero, 0) != 0)
        {
            throw new Win32Exception(Marshal.GetLastPInvokeError());
        }

        return size == sizeof(int) ? cores : throw new InvalidDataException("macOS returned an invalid physical core count.");
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetLogicalProcessorInformationEx(int relationship, [Out] byte[]? buffer, ref uint returnedLength);

    [DllImport("kernel32.dll")]
    private static extern ushort GetActiveProcessorGroupCount();

    [DllImport("libSystem.B.dylib", EntryPoint = "sysctlbyname", SetLastError = true)]
    private static extern int SysctlByName(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string name, out int oldValue, ref nuint oldLength, IntPtr newValue, nuint newLength);
}
