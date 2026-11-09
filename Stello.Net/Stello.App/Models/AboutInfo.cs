namespace Stello.App.Models;

/// <summary>The text of the About box, shared by the desktop and web versions.</summary>
public static class AboutInfo
{
    public const string Title = "About Stello";

    public const string Version = "Stello Version 4.0";

    public const string PictureCaption = "Claus Pedersen – wrote Stello in C++ in 1992 and ported it to C# in 2026";

    public static IReadOnlyList<string> Paragraphs { get; } =
    [
        "Stello is an Othello program. You play against the computer on an 8 × 8 board, and you can watch it think in the analysis panel.",
        "Its brain searches ahead with alpha-beta search, a hash table and iterative deepening. Since version 2.0 it judges " +
        "positions by patterns of squares, trained on 500 million positions from the games of Egaroucid, the Othello program by " +
        "Takuto Yamana, and its endgame solver is adapted from Egaroucid's. It knows more than 11,000 opening positions, and near " +
        "the end of the game it works out the exact result and plays perfectly.",
        "The C++ original from 1992 was ported to C# and .NET 10 in 2026, moved to Rust in version 3.0, and since version 4.0 " +
        "the search core runs in modern C++20. It can think on several threads at once (the Threads setting), and the same brain " +
        "runs in the Windows program (WPF) and in the browser (Blazor WebAssembly), where it is compiled to WebAssembly.",
        "Stello is free software under the GNU General Public License version 3 (GPL-3.0-only). The source code is at " +
        "https://github.com/cjp-dev/Stello.",
    ];
}
