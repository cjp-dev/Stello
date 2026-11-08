using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Effects;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using System.Windows.Threading;
using Stello.App.Models;
using Stello.App.ViewModels;
using Stello.Engine;

namespace Stello.Net.Views;

public partial class BoardView : UserControl
{
    private readonly Player?[] _shown = new Player?[64];
    private MainViewModel? _viewModel;
    private int _moveVersion;
    private bool _pending;
    private bool _appearanceChanged = true;

    public BoardView()
    {
        InitializeComponent();
        DataContextChanged += (_, _) => Attach();
        Loaded += (_, _) =>
        {
            Attach();
            RenderBoard();
        };
        Unloaded += (_, _) => Detach();
    }

    private void Attach()
    {
        if (_viewModel == DataContext)
        {
            return;
        }
        Detach();
        _viewModel = DataContext as MainViewModel;
        if (_viewModel is not null)
        {
            _viewModel.PropertyChanged += OnChanged;
            _moveVersion = _viewModel.BoardMoveVersion;
            UpdateAppearance();
        }
    }

    private void Detach()
    {
        if (_viewModel is not null)
        {
            _viewModel.PropertyChanged -= OnChanged;
            _viewModel = null;
        }
        Array.Clear(_shown);
        _appearanceChanged = true;
    }

    private void OnChanged(object? sender, PropertyChangedEventArgs e)
    {
        if (e.PropertyName == nameof(MainViewModel.Appearance))
        {
            UpdateAppearance();
        }
        else if (e.PropertyName != nameof(MainViewModel.MoveHistoryRows))
        {
            return;
        }
        if (_pending)
        {
            return;
        }
        _pending = true;
        Dispatcher.InvokeAsync(() =>
        {
            _pending = false;
            if (IsLoaded)
            {
                RenderBoard();
            }
        }, DispatcherPriority.Render);
    }

    private void UpdateAppearance()
    {
        Appearance appearance = _viewModel?.Appearance ?? Appearance.Default;
        bool wood = appearance.Board != BoardStyle.Green;
        Resources["BoardBrush"] = wood
            ? new ImageBrush(new BitmapImage(new Uri(
                $"pack://application:,,,/Stello.Net;component/Textures/{(appearance.Board == BoardStyle.GoldenOak ? "wood-golden-oak.jpg" : "wood-reddish.jpg")}")))
                { Stretch = Stretch.UniformToFill }
            : Brush("#145A2B");
        Resources["CellBrush"] = wood ? Brushes.Transparent : Brush("#1E7A3C");
        Resources["CellBorderBrush"] = Brush(wood ? "#BF281205" : "#0B3D1C");
        Resources["HoverCellBrush"] = Brush(wood ? "#1FFFFFFF" : "#23893F");
        Resources["CoordinateBrush"] = Brush(appearance.Board switch
        {
            BoardStyle.GoldenOak => "#4A2A10",
            BoardStyle.ReddishWood => "#F6E2C8",
            _ => "#E2E8F0",
        });
        Resources["HintBrush"] = Brush(wood ? "#BF2ECC71" : "#B32ECC71");
        Resources["BlackDiscBrush"] = appearance.Pieces3D
            ? Face("#666666", "#222222", "#000000", 0.75) : Brushes.Black;
        Resources["WhiteDiscBrush"] = appearance.Pieces3D
            ? Face("#FFFFFF", "#F2F2F2", "#C4C4C4", 0.9) : Brushes.White;
        _appearanceChanged = true;
    }

    private static SolidColorBrush Brush(string color) =>
        new((Color)ColorConverter.ConvertFromString(color));

    private static RadialGradientBrush Face(string highlight, string middle, string edge, double end) =>
        new()
        {
            Center = new Point(0.35, 0.3),
            GradientOrigin = new Point(0.35, 0.3),
            RadiusX = 0.75,
            RadiusY = 0.75,
            GradientStops =
            [
                new GradientStop((Color)ColorConverter.ConvertFromString(highlight), 0),
                new GradientStop((Color)ColorConverter.ConvertFromString(middle), 0.35),
                new GradientStop((Color)ColorConverter.ConvertFromString(edge), end),
            ],
        };

    internal Button? SquareButton(int index)
    {
        if (SquareItems.ItemContainerGenerator.ContainerFromIndex(index) is not ContentPresenter presenter)
        {
            return null;
        }
        presenter.ApplyTemplate();
        return VisualTreeHelper.GetChildrenCount(presenter) > 0
            ? VisualTreeHelper.GetChild(presenter, 0) as Button : null;
    }

    internal bool RenderBoard()
    {
        if (_viewModel is null)
        {
            return false;
        }
        var buttons = new Button[64];
        for (int i = 0; i < buttons.Length; i++)
        {
            if (SquareButton(i) is not { } button)
            {
                return false;
            }
            button.ApplyTemplate();
            buttons[i] = button;
        }

        int placed = -1;
        int placedCount = 0;
        bool changed = false;
        for (int i = 0; i < 64; i++)
        {
            Player? now = _viewModel.Squares[i].Disc;
            changed |= now != _shown[i];
            if (_shown[i] is null && now is not null)
            {
                placed = i;
                placedCount++;
            }
        }
        Appearance appearance = _viewModel.Appearance;
        bool animate = !_appearanceChanged && appearance.AnimateFlips && SystemParameters.ClientAreaAnimation
            && _moveVersion != _viewModel.BoardMoveVersion && placedCount == 1;
        bool reset = _appearanceChanged || (changed && !animate);
        for (int i = 0; i < 64; i++)
        {
            Player? now = _viewModel.Squares[i].Disc;
            Player? before = _shown[i];
            if (!reset && now == before)
            {
                continue;
            }
            var disc = (Ellipse)buttons[i].Template.FindName("Disc", buttons[i]);
            var rim = (Ellipse)buttons[i].Template.FindName("Rim", buttons[i]);
            var transform = (TransformGroup)disc.RenderTransform;
            if (transform.IsFrozen)
            {
                transform = transform.Clone();
                disc.RenderTransform = transform;
            }
            var scale = (ScaleTransform)transform.Children[0];
            var rise = (TranslateTransform)transform.Children[1];
            disc.BeginAnimation(Shape.FillProperty, null);
            rim.BeginAnimation(Shape.FillProperty, null);
            disc.BeginAnimation(OpacityProperty, null);
            scale.BeginAnimation(ScaleTransform.ScaleXProperty, null);
            scale.BeginAnimation(ScaleTransform.ScaleYProperty, null);
            rise.BeginAnimation(TranslateTransform.YProperty, null);
            disc.Visibility = now is null ? Visibility.Collapsed : Visibility.Visible;
            disc.Fill = DiscBrush(now);
            disc.StrokeThickness = appearance.Pieces3D ? 0 : 1;
            disc.Effect = appearance.Pieces3D
                ? new DropShadowEffect { Color = Colors.Black, BlurRadius = 6, ShadowDepth = 4, Direction = 270, Opacity = 0.5 }
                : null;
            rim.Visibility = now is not null && appearance.Pieces3D ? Visibility.Visible : Visibility.Collapsed;
            rim.Fill = RimBrush(now);
            if (animate && now is not null && now != before)
            {
                if (before is null)
                {
                    disc.BeginAnimation(OpacityProperty, Placement(0, 1));
                    scale.BeginAnimation(ScaleTransform.ScaleXProperty, Placement(1.3, 1));
                    scale.BeginAnimation(ScaleTransform.ScaleYProperty, Placement(1.3, 1));
                    rise.BeginAnimation(TranslateTransform.YProperty, Placement(-6, 0));
                }
                else
                {
                    int distance = Math.Max(Math.Abs(i / 8 - placed / 8), Math.Abs(i % 8 - placed % 8));
                    int delay = 150 + (distance - 1) * 70;
                    scale.BeginAnimation(ScaleTransform.ScaleXProperty, Flip(1, 0, delay));
                    scale.BeginAnimation(ScaleTransform.ScaleYProperty, Flip(1, 1.15, delay));
                    rise.BeginAnimation(TranslateTransform.YProperty, Flip(0, -5, delay));
                    var face = new ObjectAnimationUsingKeyFrames
                    {
                        Duration = TimeSpan.FromMilliseconds(delay + 450),
                        FillBehavior = FillBehavior.Stop,
                    };
                    face.KeyFrames.Add(new DiscreteObjectKeyFrame(DiscBrush(before), KeyTime.FromTimeSpan(TimeSpan.Zero)));
                    face.KeyFrames.Add(new DiscreteObjectKeyFrame(DiscBrush(now), KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(delay + 225))));
                    disc.BeginAnimation(Shape.FillProperty, face);
                    var edge = new ObjectAnimationUsingKeyFrames
                    {
                        Duration = face.Duration,
                        FillBehavior = FillBehavior.Stop,
                    };
                    edge.KeyFrames.Add(new DiscreteObjectKeyFrame(RimBrush(before), KeyTime.FromTimeSpan(TimeSpan.Zero)));
                    edge.KeyFrames.Add(new DiscreteObjectKeyFrame(RimBrush(now), KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(delay + 225))));
                    rim.BeginAnimation(Shape.FillProperty, edge);
                }
            }
            _shown[i] = now;
        }
        _appearanceChanged = false;
        _moveVersion = _viewModel.BoardMoveVersion;
        return true;
    }

    private Brush DiscBrush(Player? player) =>
        (Brush)Resources[player == Player.White ? "WhiteDiscBrush" : "BlackDiscBrush"];

    private static Brush RimBrush(Player? player) => Brush(player == Player.Black ? "#D8D8D8" : "#111111");

    private static DoubleAnimation Placement(double from, double to) =>
        new(from, to, TimeSpan.FromMilliseconds(250)) { FillBehavior = FillBehavior.Stop };

    private static DoubleAnimationUsingKeyFrames Flip(double resting, double midpoint, int delay)
    {
        var animation = new DoubleAnimationUsingKeyFrames
        {
            Duration = TimeSpan.FromMilliseconds(delay + 450),
            FillBehavior = FillBehavior.Stop,
        };
        animation.KeyFrames.Add(new LinearDoubleKeyFrame(resting, KeyTime.FromTimeSpan(TimeSpan.Zero)));
        animation.KeyFrames.Add(new LinearDoubleKeyFrame(resting, KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(delay))));
        animation.KeyFrames.Add(new EasingDoubleKeyFrame(midpoint, KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(delay + 225)))
            { EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut } });
        animation.KeyFrames.Add(new EasingDoubleKeyFrame(resting, KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(delay + 450)))
            { EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut } });
        return animation;
    }
}
