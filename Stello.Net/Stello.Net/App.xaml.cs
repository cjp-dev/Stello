using System.IO;
using System.Windows;
using Stello.Engine;
using Stello.Net.Models;
using Stello.Net.Services;
using Stello.Net.ViewModels;

namespace Stello.Net;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        OpeningBook? book = null;
        string? notice = null;
        try
        {
            book = OpeningBook.Load(Path.Combine(AppContext.BaseDirectory, "Data", "OPENING"));
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            notice = "The opening book could not be loaded.";
        }

        var computer = new ComputerPlayer(new SearchEngine(), book, new Random());
        var viewModel = new MainViewModel(computer, new DialogService(), GameSettings.Default, notice);
        MainWindow = new MainWindow(viewModel);
        MainWindow.Show();
    }
}
