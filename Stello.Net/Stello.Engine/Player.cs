namespace Stello.Engine;

// C++: DARK = Black, LIGHT = White.
public enum Player
{
    Black,
    White,
}

public static class PlayerExtensions
{
    public static Player Opponent(this Player player) =>
        player == Player.Black ? Player.White : Player.Black;
}
