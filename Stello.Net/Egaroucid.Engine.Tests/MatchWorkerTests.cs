// SPDX-License-Identifier: GPL-3.0-only

using System.Buffers.Binary;
using Stello.Benchmark;
using Stello.BookTool;
using Stello.Engine;
using Xunit.Abstractions;

namespace Stello.Egaroucid.Tests;

public class MatchWorkerTests(ITestOutputHelper output)
{
    [Theory]
    [InlineData(16, 1, 1, 16)]
    [InlineData(16, 8, 8, 2)]
    [InlineData(16, 1, 8, 2)]
    [InlineData(16, 8, 1, 2)]
    [InlineData(10, 8, 8, 1)]
    [InlineData(4, 8, 8, 1)]
    [InlineData(24, 1, 1, 24)]
    public void WorkerCount_DefaultUsesPhysicalCoresAndTheLargerSide(int cores, int threadsA, int threadsB, int expected)
    {
        MatchSide a = MatchCommands.ParseSide($"depth=1,threads={threadsA}");
        MatchSide b = MatchCommands.ParseSide($"depth=1,threads={threadsB}");

        Assert.Equal(expected, MatchCommands.WorkerCount(Options.Parse([]), a, b, () => cores));
    }

    [Theory]
    [InlineData("1", 1)]
    [InlineData("9", 9)]
    [InlineData("256", 256)]
    public void WorkerCount_ExplicitWorkersOverrideTheDefaultWithoutDetectingCores(string value, int expected)
    {
        MatchSide a = MatchCommands.ParseSide("depth=1,threads=8");
        MatchSide b = MatchCommands.ParseSide("depth=1");
        Options options = Options.Parse(["--workers", value]);

        Assert.Equal(expected, MatchCommands.WorkerCount(options, a, b, () => throw new InvalidOperationException("Core detection was called.")));
    }

    [Theory]
    [InlineData("0")]
    [InlineData("257")]
    [InlineData("invalid")]
    public void WorkerCount_RejectsInvalidExplicitWorkers(string value)
    {
        MatchSide side = MatchCommands.ParseSide("depth=1");
        Options options = Options.Parse(["--workers", value]);

        Assert.Throws<ArgumentException>(() => MatchCommands.WorkerCount(options, side, side, () => throw new InvalidOperationException("Core detection was called.")));
    }

    [Fact]
    public void WorkerCount_UsesTheDetectedPhysicalCores()
    {
        int cores = ProcessorTopology.PhysicalCores();
        MatchSide one = MatchCommands.ParseSide("depth=1");
        MatchSide eight = MatchCommands.ParseSide("depth=1,threads=8");
        int oneThreadWorkers = MatchCommands.WorkerCount(Options.Parse([]), one, one, ProcessorTopology.PhysicalCores);
        int eightThreadWorkers = MatchCommands.WorkerCount(Options.Parse([]), eight, eight, ProcessorTopology.PhysicalCores);

        Assert.InRange(cores, 1, Environment.ProcessorCount);
        Assert.Equal(cores, oneThreadWorkers);
        Assert.Equal(Math.Max(1, cores / 8), eightThreadWorkers);
        output.WriteLine($"Detected {cores} physical cores and {Environment.ProcessorCount} available logical processors; defaults: {oneThreadWorkers} workers at 1 thread, {eightThreadWorkers} at 8 threads.");
    }

    [Fact]
    public void WindowsTopology_CountsHybridCoresAndRespectsAffinity()
    {
        byte[] topology = WindowsTopology(0b0011, 0b0100, 0b1000);

        Assert.Equal(3, ProcessorTopology.CountWindowsCores(topology, null));
        Assert.Equal(1, ProcessorTopology.CountWindowsCores(topology, 0b0011));
        Assert.Equal(2, ProcessorTopology.CountWindowsCores(topology, 0b0101));
    }

    [Fact]
    public void WindowsTopology_RejectsMalformedRecords()
    {
        byte[] topology = WindowsTopology(1);
        BinaryPrimitives.WriteUInt32LittleEndian(topology.AsSpan(4), 0);

        Assert.Throws<InvalidDataException>(() => ProcessorTopology.CountWindowsCores(topology, null));
    }

    [Theory]
    [InlineData("0-3", "0-3", 3)]
    [InlineData("0-3", "0,2", 1)]
    [InlineData("0-3", "1-3", 3)]
    [InlineData("0-2", "0-3", 2)]
    public void LinuxTopology_CountsHybridCoresAndRespectsAffinity(string online, string allowed, int expected)
    {
        var siblings = new Dictionary<int, string> { [0] = "0,2", [1] = "1", [2] = "0,2", [3] = "3" };

        Assert.Equal(expected, ProcessorTopology.CountLinuxCores(online, allowed, cpu => siblings[cpu]));
    }

    [Theory]
    [InlineData("")]
    [InlineData("2-1")]
    [InlineData("0,invalid")]
    public void LinuxTopology_RejectsInvalidCpuLists(string online)
    {
        Assert.Throws<InvalidDataException>(() => ProcessorTopology.CountLinuxCores(online, "0-3", _ => "0"));
    }

    [Fact]
    public void LinuxTopology_RejectsInconsistentOrUnavailableCpus()
    {
        Assert.Throws<InvalidDataException>(() => ProcessorTopology.CountLinuxCores("0-1", "0-1", _ => "2"));
        Assert.Throws<InvalidDataException>(() => ProcessorTopology.CountLinuxCores("0-1", "2-3", _ => "0"));
    }

    private static byte[] WindowsTopology(params ulong[] masks)
    {
        int recordSize = 32 + IntPtr.Size + 8;
        byte[] topology = new byte[recordSize * masks.Length];
        for (int i = 0; i < masks.Length; i++)
        {
            Span<byte> record = topology.AsSpan(i * recordSize, recordSize);
            BinaryPrimitives.WriteUInt32LittleEndian(record[4..], (uint)recordSize);
            BinaryPrimitives.WriteUInt16LittleEndian(record[30..], 1);
            if (IntPtr.Size == 8)
            {
                BinaryPrimitives.WriteUInt64LittleEndian(record[32..], masks[i]);
            }
            else
            {
                BinaryPrimitives.WriteUInt32LittleEndian(record[32..], (uint)masks[i]);
            }
        }

        return topology;
    }
}
