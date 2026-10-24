namespace Stello.App.Models;

public enum GameMode
{
    HumanVsComputer,   // Human plays Black (moves 1st)
    ComputerVsHuman,   // Computer plays Black (moves 1st), Human plays White
    HumanVsHuman,      // Local two-player hot-seat
    ComputerVsComputer // AI vs AI demonstration
}

