// SPDX-License-Identifier: GPL-3.0-only

namespace Stello.Egaroucid.Tests;

/// <summary>
/// A test that needs Egaroucid's resource files (not in the repository). It is skipped when they cannot be found; see
/// <see cref="EgaroucidResources.FindDirectory"/>.
/// </summary>
public sealed class EgaroucidFactAttribute : FactAttribute
{
    public EgaroucidFactAttribute()
    {
        if (EgaroucidResources.FindDirectory() is null)
        {
            Skip = "Egaroucid's resources were not found (Egaroucid-main/bin/resources or EGAROUCID_RESOURCES)";
        }
    }
}

/// <summary>The fixtures written by tools/Egaroucid.Reference/make-fixtures.sh from the C++ Egaroucid.</summary>
internal static class Fixtures
{
    public static string PathOf(string name) => Path.Combine(AppContext.BaseDirectory, "Data", "egaroucid", name);

    /// <summary>The tab-separated fields of every line that is not a comment ("#") or empty.</summary>
    public static IEnumerable<string[]> Rows(string name) =>
        File.ReadLines(PathOf(name))
            .Where(line => line.Length > 0 && line[0] != '#')
            .Select(line => line.Split('\t'));

    /// <summary>Loads the resources for a test (once per test run).</summary>
    public static void LoadResources() => EgaroucidResources.Load();
}
