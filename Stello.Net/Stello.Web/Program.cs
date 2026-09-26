using Microsoft.AspNetCore.Components.Web;
using Microsoft.AspNetCore.Components.WebAssembly.Hosting;
using Microsoft.JSInterop;
using Stello.App.Services;
using Stello.App.ViewModels;
using Stello.Engine;
using Stello.Web;
using Stello.Web.Services;

var builder = WebAssemblyHostBuilder.CreateDefault(args);
builder.RootComponents.Add<App>("#app");
builder.RootComponents.Add<HeadOutlet>("head::after");

builder.Services.AddSingleton(new HttpClient { BaseAddress = new Uri(builder.HostEnvironment.BaseAddress) });
builder.Services.AddSingleton(services => (IJSInProcessRuntime)services.GetRequiredService<IJSRuntime>());
builder.Services.AddSingleton<BrowserDialogService>();
builder.Services.AddSingleton<IDialogService>(services => services.GetRequiredService<BrowserDialogService>());
builder.Services.AddSingleton<IGameFileService, BrowserGameFileService>();
builder.Services.AddSingleton<ISettingsStore, LocalStorageSettingsStore>();
builder.Services.AddSingleton<LocalStorageBookStore>();
builder.Services.AddSingleton<StartupBook>();
builder.Services.AddSingleton<IEngineHost>(services =>
{
    OpeningBook book = services.GetRequiredService<StartupBook>().Book;
    return new LocalEngineHost(
        new ComputerPlayer(new SearchEngine(), book, new Random()),
        book,
        services.GetRequiredService<LocalStorageBookStore>());
});
builder.Services.AddSingleton(services => new MainViewModel(
    services.GetRequiredService<IEngineHost>(),
    services.GetRequiredService<IDialogService>(),
    services.GetRequiredService<IGameFileService>(),
    services.GetRequiredService<ISettingsStore>(),
    services.GetRequiredService<StartupBook>().Notice));

WebAssemblyHost host = builder.Build();

// The engine host and the view model need the book, so it is loaded before the first page is shown.
await host.Services.GetRequiredService<StartupBook>().LoadAsync();
await host.RunAsync();
