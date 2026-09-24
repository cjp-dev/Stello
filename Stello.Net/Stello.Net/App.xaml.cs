using System.Windows;
using Stello.Engine;
using Stello.Net.Services;
using Stello.Net.ViewModels;

namespace Stello.Net;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        OpeningBook book = BookLoader.Load(AppPaths.BookFiles, out string? notice) ?? OpeningBook.CreateEmpty();
        var computer = new ComputerPlayer(new SearchEngine(), book, new Random());
        var viewModel = new MainViewModel(
            computer,
            book,
            new FileBookStore(AppPaths.UserBookFile, AppPaths.SelfPlayLogFile),
            new DialogService(),
            new JsonSettingsStore(AppPaths.SettingsFile),
            notice);
        MainWindow = new MainWindow(viewModel);
        MainWindow.Show();
    }
}
