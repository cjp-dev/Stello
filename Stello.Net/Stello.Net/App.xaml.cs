using System.Windows;
using Stello.App.Services;
using Stello.App.ViewModels;
using Stello.Engine;
using Stello.Net.Services;

namespace Stello.Net;

public partial class App : Application
{
    private SoundService? _sound;
    private MainViewModel? _viewModel;
    private int _soundErrorReported;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        OpeningBook book = BookLoader.Load(AppPaths.BookFiles, out string? notice) ?? OpeningBook.CreateEmpty();
        var engine = new LocalEngineHost(
            new ComputerPlayer(new SearchEngine(), book, new Random()),
            book,
            new FileBookStore(AppPaths.UserBookFile, AppPaths.SelfPlayLogFile));
        var dialogs = new DialogService();
        _sound = new SoundService(exception =>
        {
            if (Interlocked.Exchange(ref _soundErrorReported, 1) == 0)
            {
                Dispatcher.InvokeAsync(() => dialogs.ShowError($"Sound could not be played. You can disable it in Options.\n\n{exception.Message}"));
            }
        });
        _viewModel = new MainViewModel(
            engine,
            dialogs,
            new GameFileService(),
            new JsonSettingsStore(AppPaths.SettingsFile),
            notice,
            _sound);
        MainWindow = new MainWindow(_viewModel);
        MainWindow.Show();
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _viewModel?.Dispose();
        _sound?.Dispose();
        base.OnExit(e);
    }
}
