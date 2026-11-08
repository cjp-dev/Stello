using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using System.Windows.Threading;
using Stello.App.Models;
using Stello.App.Services;
using Stello.App.ViewModels;
using Stello.Engine;
using Stello.Net.Views;

namespace Stello.Net.Tests;

public sealed class BoardAndHelpTests
{
    [Fact]
    public Task Board_UsesSharedTexturesFacesAndHintColors() => OnUiThread(() =>
    {
        using MainViewModel vm = Create();
        var board = CreateBoard(vm);
        foreach (BoardStyle style in Enum.GetValues<BoardStyle>())
        {
            vm.Appearance = new Appearance(style, Pieces3D: true, AnimateFlips: false);
            Assert.True(board.RenderBoard());
            if (style == BoardStyle.Green)
            {
                Assert.Equal(Color.FromRgb(20, 90, 43), ((SolidColorBrush)board.Resources["BoardBrush"]).Color);
                Assert.Equal(Color.FromRgb(30, 122, 60), ((SolidColorBrush)board.Resources["CellBrush"]).Color);
            }
            else
            {
                var image = Assert.IsType<BitmapImage>(((ImageBrush)board.Resources["BoardBrush"]).ImageSource);
                Assert.True(image.PixelWidth > 0);
                Assert.EndsWith(style == BoardStyle.GoldenOak ? "wood-golden-oak.jpg" : "wood-reddish.jpg", image.UriSource.AbsoluteUri);
                Assert.Equal(Colors.Transparent, ((SolidColorBrush)board.Resources["CellBrush"]).Color);
            }
            Assert.Equal(Color.FromArgb(style == BoardStyle.Green ? (byte)179 : (byte)191, 46, 204, 113),
                ((SolidColorBrush)board.Resources["HintBrush"]).Color);
            Ellipse black = Shape(board, "d5", "Disc");
            Ellipse white = Shape(board, "d4", "Disc");
            Assert.IsType<RadialGradientBrush>(black.Fill);
            Assert.IsType<RadialGradientBrush>(white.Fill);
            Assert.Equal(0, black.StrokeThickness);
            Assert.NotNull(black.Effect);
            Assert.Equal(Visibility.Visible, Shape(board, "d5", "Rim").Visibility);
        }
        Assert.Equal(Color.FromRgb(246, 226, 200), ((SolidColorBrush)board.Resources["CoordinateBrush"]).Color);
        Assert.Equal(Color.FromRgb(231, 76, 60), ((SolidColorBrush)Shape(board, "d3", "Last").Fill).Color);

        vm.Appearance = Appearance.Classic;
        Assert.True(board.RenderBoard());
        Assert.IsType<SolidColorBrush>(Shape(board, "d5", "Disc").Fill);
        Assert.Equal(Colors.Black, ((SolidColorBrush)Shape(board, "d5", "Disc").Fill).Color);
        Assert.Equal(1, Shape(board, "d5", "Disc").StrokeThickness);
        Assert.Null(Shape(board, "d5", "Disc").Effect);
        Assert.Equal(Visibility.Collapsed, Shape(board, "d5", "Rim").Visibility);
    });

    [Fact]
    public Task Board_AnimatesActualMovesButNotNavigationOrAppearanceChanges() => OnUiThread(() =>
    {
        using MainViewModel vm = Create();
        vm.SetGameMode(GameMode.HumanVsHuman).GetAwaiter().GetResult();
        var board = CreateBoard(vm);
        int initialVersion = vm.BoardMoveVersion;
        vm.PlayCommand.Execute(vm.Squares[Square.Parse("d3").Index]);
        vm.Idle.GetAwaiter().GetResult();
        Assert.Equal(initialVersion + 1, vm.BoardMoveVersion);
        Assert.True(board.RenderBoard());
        Ellipse flipped = Shape(board, "d4", "Disc");
        var scale = (ScaleTransform)((TransformGroup)flipped.RenderTransform).Children[0];
        Assert.Equal(SystemParameters.ClientAreaAnimation, scale.HasAnimatedProperties);
        if (SystemParameters.ClientAreaAnimation)
        {
            Pump(TimeSpan.FromMilliseconds(40));
            Assert.Equal(((RadialGradientBrush)board.Resources["WhiteDiscBrush"]).GradientStops.Select(stop => stop.Color),
                ((RadialGradientBrush)flipped.Fill).GradientStops.Select(stop => stop.Color));
            Assert.Equal(((RadialGradientBrush)board.Resources["BlackDiscBrush"]).GradientStops.Select(stop => stop.Color),
                ((RadialGradientBrush)flipped.GetAnimationBaseValue(System.Windows.Shapes.Shape.FillProperty)).GradientStops.Select(stop => stop.Color));
            Pump(TimeSpan.FromMilliseconds(650));
            Assert.Equal(((RadialGradientBrush)board.Resources["BlackDiscBrush"]).GradientStops.Select(stop => stop.Color),
                ((RadialGradientBrush)flipped.Fill).GradientStops.Select(stop => stop.Color));
            Assert.Equal(1, scale.ScaleX);
            Assert.Equal(1, scale.ScaleY);
            Assert.Equal(0, ((TranslateTransform)((TransformGroup)flipped.RenderTransform).Children[1]).Y);
        }
        Assert.Equal(Visibility.Visible, Shape(board, "d3", "Last").Visibility);

        vm.UndoCommand.ExecuteAsync(null).GetAwaiter().GetResult();
        vm.Idle.GetAwaiter().GetResult();
        Assert.True(board.RenderBoard());
        Assert.False(scale.HasAnimatedProperties);
        Assert.Equal(initialVersion + 1, vm.BoardMoveVersion);
        vm.RedoCommand.ExecuteAsync(null).GetAwaiter().GetResult();
        vm.Idle.GetAwaiter().GetResult();
        Assert.True(board.RenderBoard());
        Assert.False(scale.HasAnimatedProperties);
        Assert.Equal(initialVersion + 1, vm.BoardMoveVersion);

        vm.PlayCommand.Execute(vm.Squares[Square.Parse("c3").Index]);
        vm.Idle.GetAwaiter().GetResult();
        Assert.True(board.RenderBoard());
        vm.Appearance = vm.Appearance with { AnimateFlips = false };
        Assert.True(board.RenderBoard());
        Assert.All(vm.Squares, square =>
        {
            Ellipse disc = Shape(board, square.Name, "Disc");
            Assert.False(((TransformGroup)disc.RenderTransform).Children[0].HasAnimatedProperties);
        });
        vm.NewGameCommand.ExecuteAsync(null).GetAwaiter().GetResult();
        vm.Idle.GetAwaiter().GetResult();
        Assert.True(board.RenderBoard());
        Assert.False(scale.HasAnimatedProperties);
    });

    [Fact]
    public Task Help_MenuAndShortcutsOpenLiveDocumentation() => OnUiThread(() =>
    {
        using MainViewModel vm = Create();
        var opened = new List<Uri>();
        var window = new MainWindow(vm, opened.Add);
        KeyBinding[] bindings = window.InputBindings.OfType<KeyBinding>().Where(binding => binding.Key == Key.F1).ToArray();
        Assert.Equal(2, bindings.Length);
        Assert.Same(MainWindow.UserGuideCommand, bindings.Single(binding => binding.Modifiers == ModifierKeys.None).Command);
        Assert.Same(MainWindow.DocumentationCommand, bindings.Single(binding => binding.Modifiers == ModifierKeys.Shift).Command);
        MainWindow.UserGuideCommand.Execute(null, window);
        MainWindow.DocumentationCommand.Execute(null, window);
        Assert.Equal(
            ["https://polite-sky-0e9899910.3.azurestaticapps.net/docs/15-gui-and-settings",
             "https://polite-sky-0e9899910.3.azurestaticapps.net/docs/"],
            opened.Select(uri => uri.AbsoluteUri));
        var menu = (Menu)((DockPanel)window.Content).Children[0];
        var help = menu.Items.OfType<MenuItem>().Single(item => (string)item.Header == "_Help");
        Assert.Same(MainWindow.UserGuideCommand, ((MenuItem)help.Items[0]).Command);
        Assert.Same(MainWindow.DocumentationCommand, ((MenuItem)help.Items[1]).Command);
    });

    private static BoardView CreateBoard(MainViewModel vm)
    {
        var board = new BoardView { DataContext = vm };
        board.Measure(new Size(440, 440));
        board.Arrange(new Rect(0, 0, 440, 440));
        board.UpdateLayout();
        Assert.True(board.RenderBoard());
        return board;
    }

    private static Ellipse Shape(BoardView board, string square, string name)
    {
        Button button = Assert.IsType<Button>(board.SquareButton(Square.Parse(square).Index));
        return Assert.IsType<Ellipse>(button.Template.FindName(name, button));
    }

    private static MainViewModel Create()
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        return new MainViewModel(
            new LocalEngineHost(new ComputerPlayer(new SearchEngine(12), book, new Random(0)), book, new FakeBookStore()),
            new FakeDialogService(), new FakeGameFileService(), new FakeSettingsStore(AppSettings.Default));
    }

    private static async Task OnUiThread(Action action)
    {
        var completion = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        var thread = new Thread(() =>
        {
            try
            {
                action();
                completion.SetResult();
            }
            catch (Exception exception)
            {
                completion.SetException(exception);
            }
        }) { IsBackground = true };
        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        await completion.Task.WaitAsync(TimeSpan.FromSeconds(15));
    }

    private static void Pump(TimeSpan duration)
    {
        var frame = new DispatcherFrame();
        var timer = new DispatcherTimer { Interval = duration };
        timer.Tick += (_, _) =>
        {
            timer.Stop();
            frame.Continue = false;
        };
        timer.Start();
        Dispatcher.PushFrame(frame);
    }
}
