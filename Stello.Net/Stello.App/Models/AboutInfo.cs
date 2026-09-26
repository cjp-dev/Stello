namespace Stello.App.Models;

/// <summary>The text of the About box, shared by the desktop and web versions.</summary>
public static class AboutInfo
{
    public const string Title = "About Stello";

    public const string Version = "Stello Version 2.0";

    public const string Copyright = "Copyright (C) 1992 Futuresoft";

    public const string PictureCaption = "Claus Pedersen – wrote Stello in C++ in 1992 and ported it to C# in 2026";

    public static IReadOnlyList<string> Paragraphs { get; } =
    [
        "Stello is an Othello program. You play against the computer on an 8 × 8 board, and you can watch it think in the analysis panel.",
        "Its brain searches ahead with alpha-beta search, a hash table and iterative deepening, and judges positions by their edges, " +
        "corners and mobility. It knows more than 23,000 opening positions, and near the end of the game it works out the exact " +
        "result and plays perfectly.",
        "The C++ original from 1992 was ported to C# and .NET 10 in 2026. The same brain now runs as a Windows program (WPF) and " +
        "in the browser (Blazor WebAssembly).",
    ];
}
