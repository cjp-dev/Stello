using System.Globalization;
using Stello.BookTool;

namespace Stello.Trainer;

/// <summary>
/// Command line tool that trains Stello's pattern evaluation from Egaroucid's training data
/// (Plans/improve-stello-strength-plan.md, phases 1 and 2).
/// </summary>
internal static class TrainerProgram
{
    private const string Usage = """
        Usage: Stello.Trainer <command> [options]

          convert [--data <folder>] [--out <folder>] [--cap 10000000] [--workers N] [--max-files N]
                                    Convert Egaroucid's training data (default: Traning data) into binary files of
                                    positions with final scores, split into training, validation and test
                                    (default: Traning data/stello-bin). Never commit or share the result.
          train --out <file> [--set classic11] [--stages 15] [--data <folder>] [--max-per-discs N] [--smooth 2]
                [--loops 1000] [--rate 0.5] [--patience 30] [--workers N] [--only-stage S]
                                    Train pattern weights stage by stage and write an evaluation file.
          report --model <file> [--data <folder>] [--max-per-discs N] [--name <name>]
                                    The model's error on the test positions per number of empty squares, next to
                                    Stello 1 (scaled) and the Egaroucid port.
        """;

    public static int Main(string[] args)
    {
        CultureInfo.CurrentCulture = CultureInfo.InvariantCulture;
        if (args.Length == 0)
        {
            return Fail(Usage);
        }

        try
        {
            Options options = Options.Parse(args[1..]);
            if (options.Positional.Count != 0)
            {
                return Fail(Usage);
            }

            return args[0] switch
            {
                "convert" => ConvertCommand.Run(options),
                "train" => TrainCommand.Run(options),
                "report" => ReportCommand.Run(options),
                "size" => SizeCommand.Run(options),
                _ => Fail(Usage),
            };
        }
        catch (ArgumentException exception)
        {
            return Fail(exception.Message);
        }
        catch (Exception exception) when (exception is InvalidDataException or FormatException or IOException or UnauthorizedAccessException)
        {
            return Fail(exception.Message);
        }
    }

    /// <summary>The Stello repository folder: the first folder above the current folder or the tool with Stello.Net/Stello.Net.slnx.</summary>
    public static string RepositoryFolder()
    {
        foreach (string start in new[] { Environment.CurrentDirectory, AppContext.BaseDirectory })
        {
            for (DirectoryInfo? folder = new(start); folder is not null; folder = folder.Parent)
            {
                if (File.Exists(Path.Combine(folder.FullName, "Stello.Net", "Stello.Net.slnx")))
                {
                    return folder.FullName;
                }
            }
        }

        return Environment.CurrentDirectory;
    }

    private static int Fail(string message)
    {
        Console.Error.WriteLine(message);
        return 1;
    }
}
