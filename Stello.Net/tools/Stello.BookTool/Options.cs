using System.Globalization;
using Stello.Engine;

namespace Stello.BookTool;

/// <summary>The arguments after the command: positional arguments and <c>--name value</c> options.</summary>
internal sealed class Options
{
    private readonly Dictionary<string, string> _named = new(StringComparer.Ordinal);
    private readonly List<string> _positional = [];

    public IReadOnlyList<string> Positional => _positional;

    public string this[int index] => _positional[index];

    /// <exception cref="ArgumentException">An option has no value or is given twice.</exception>
    public static Options Parse(IEnumerable<string> args)
    {
        var options = new Options();
        using IEnumerator<string> arg = args.GetEnumerator();
        while (arg.MoveNext())
        {
            if (!arg.Current.StartsWith("--", StringComparison.Ordinal))
            {
                options._positional.Add(arg.Current);
                continue;
            }

            string name = arg.Current[2..];
            if (!arg.MoveNext() || !options._named.TryAdd(name, arg.Current))
            {
                throw new ArgumentException($"--{name} needs one value.");
            }
        }

        return options;
    }

    /// <exception cref="ArgumentException">An option that the command does not know was given.</exception>
    public void Allow(params string[] names)
    {
        if (_named.Keys.FirstOrDefault(name => !names.Contains(name)) is { } unknown)
        {
            throw new ArgumentException($"Unknown option --{unknown}.");
        }
    }

    public string? Text(string name) => _named.GetValueOrDefault(name);

    /// <exception cref="ArgumentException">The value is not a whole number in the range.</exception>
    public int Number(string name, int defaultValue, int min, int max)
    {
        if (!_named.TryGetValue(name, out string? text))
        {
            return defaultValue;
        }

        return int.TryParse(text, NumberStyles.None, CultureInfo.InvariantCulture, out int value) && value >= min && value <= max
            ? value
            : throw new ArgumentException($"--{name} must be a whole number from {min} to {max}.");
    }

    /// <summary>The search limit: <c>--depth N</c> or <c>--time-s S</c>, else <paramref name="defaultLimits"/>.</summary>
    /// <exception cref="ArgumentException">Both are given, or a value is not valid.</exception>
    public SearchLimits Limits(SearchLimits defaultLimits)
    {
        string? time = Text("time-s");
        if (time is not null && Text("depth") is not null)
        {
            throw new ArgumentException("Give either --time-s or --depth.");
        }

        if (time is not null)
        {
            return double.TryParse(time, NumberStyles.AllowDecimalPoint, CultureInfo.InvariantCulture, out double seconds) && seconds > 0
                ? SearchLimits.TimePerMove(TimeSpan.FromSeconds(seconds))
                : throw new ArgumentException("--time-s must be a positive number of seconds.");
        }

        return Text("depth") is null ? defaultLimits : SearchLimits.FixedDepth(Number("depth", 1, 1, 60));
    }
}
