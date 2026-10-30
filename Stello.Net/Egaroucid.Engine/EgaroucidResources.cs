// SPDX-License-Identifier: GPL-3.0-only
// Part of the C# port of Egaroucid (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later.

namespace Stello.Egaroucid;

/// <summary>
/// Finds and loads Egaroucid's resource files: the evaluation (<c>eval.egev2</c>, <c>eval_move_ordering_end.egev</c>)
/// and the hash numbers (<c>hash/hash25.eghs</c> .. <c>hash27.eghs</c>). They are not part of this repository; by
/// default they are read from <c>Egaroucid-main/bin/resources</c> in the repository root (see THIRD-PARTY-NOTICES.md).
/// </summary>
internal static class EgaroucidResources
{
    /// <summary>The environment variable that overrides the resource directory.</summary>
    public const string EnvironmentVariable = "EGAROUCID_RESOURCES";

    private static readonly Lock Gate = new();

    public static string? LoadedDirectory { get; private set; }

    /// <summary>
    /// The resource directory: <see cref="EnvironmentVariable"/> if set, otherwise <c>Egaroucid-main/bin/resources</c> in
    /// the first parent of the application directory that has it; null when there is none.
    /// </summary>
    public static string? FindDirectory()
    {
        string? fromEnvironment = Environment.GetEnvironmentVariable(EnvironmentVariable);
        if (!string.IsNullOrEmpty(fromEnvironment))
        {
            return IsResourceDirectory(fromEnvironment) ? Path.GetFullPath(fromEnvironment) : null;
        }

        for (var directory = new DirectoryInfo(AppContext.BaseDirectory); directory is not null; directory = directory.Parent)
        {
            string candidate = Path.Combine(directory.FullName, "Egaroucid-main", "bin", "resources");
            if (IsResourceDirectory(candidate))
            {
                return candidate;
            }
        }

        return null;
    }

    public static bool IsResourceDirectory(string directory) =>
        File.Exists(Path.Combine(directory, "eval.egev2")) && File.Exists(Path.Combine(directory, "eval_move_ordering_end.egev"));

    /// <summary>Loads the resources once; later calls with the same directory and hash level do nothing.</summary>
    /// <param name="directory">The resource directory, or null for <see cref="FindDirectory"/>.</param>
    /// <exception cref="DirectoryNotFoundException">No resource directory was found.</exception>
    /// <exception cref="InvalidOperationException">Other resources or another hash level are already loaded.</exception>
    public static void Load(string? directory = null, int hashLevel = HashTables.DefaultHashLevel)
    {
        directory ??= FindDirectory() ?? throw new DirectoryNotFoundException(
            $"Egaroucid's resources were not found. Put Egaroucid in <repo>/Egaroucid-main or set {EnvironmentVariable}.");
        directory = Path.GetFullPath(directory);
        lock (Gate)
        {
            if (LoadedDirectory is not null)
            {
                if (LoadedDirectory != directory || HashTables.Level != hashLevel)
                {
                    throw new InvalidOperationException($"Egaroucid's resources are already loaded from {LoadedDirectory} with hash level {HashTables.Level}");
                }

                return;
            }

            Evaluate.Load(Path.Combine(directory, "eval.egev2"), Path.Combine(directory, "eval_move_ordering_end.egev"));
            HashTables.Load(directory, hashLevel);
            LoadedDirectory = directory;
        }
    }
}
