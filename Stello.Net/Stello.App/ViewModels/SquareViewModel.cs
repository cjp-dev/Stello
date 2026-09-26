using CommunityToolkit.Mvvm.ComponentModel;
using Stello.Engine;

namespace Stello.App.ViewModels;

public sealed partial class SquareViewModel(Square square) : ObservableObject
{
    [ObservableProperty]
    private Player? _disc;

    [ObservableProperty]
    private bool _isLegalMove;

    [ObservableProperty]
    private bool _isLastMove;

    public Square Square { get; } = square;

    public string Name => Square.ToString();
}
