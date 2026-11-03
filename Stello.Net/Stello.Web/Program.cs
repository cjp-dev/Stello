using Microsoft.AspNetCore.Components.Web;
using Microsoft.AspNetCore.Components.WebAssembly.Hosting;
using Microsoft.JSInterop;
using Stello.App.Models;
using Stello.App.Services;
using Stello.App.ViewModels;
using Stello.Web;
using Stello.Web.Services;
using GameShell;

var builder = WebAssemblyHostBuilder.CreateDefault(args);
builder.RootComponents.Add<App>("#app");
builder.RootComponents.Add<HeadOutlet>("head::after");

builder.Services.AddSingleton(new HttpClient { BaseAddress = new Uri(builder.HostEnvironment.BaseAddress) });
builder.Services.AddSingleton(services => (IJSInProcessRuntime)services.GetRequiredService<IJSRuntime>());
builder.Services.AddSingleton<ShellDialogs>();
// The Threads setting's range and default: navigator.hardwareConcurrency.
builder.Services.AddSingleton(services =>
    ThreadCounts.ForBrowser(services.GetRequiredService<IJSInProcessRuntime>().Invoke<int>("stello.hardwareConcurrency")));
builder.Services.AddSingleton<BrowserDialogService>();
builder.Services.AddSingleton<IDialogService>(services => services.GetRequiredService<BrowserDialogService>());
builder.Services.AddSingleton<IGameFileService, BrowserGameFileService>();
builder.Services.AddSingleton<ISoundService, BrowserSoundService>();
builder.Services.AddSingleton<ISettingsStore, LocalStorageSettingsStore>();
builder.Services.AddSingleton<LocalStorageBookStore>();
builder.Services.AddSingleton<StartupBook>();
builder.Services.AddBrainDocs("The Stello brain", "Stello", "https://github.com/cjp-dev/Stello/blob/main/docs/");
builder.Services.AddSingleton<AppearanceStore>();
builder.Services.AddSingleton<IEngineHost>(services =>
{
    ThreadCounts threads = services.GetRequiredService<ThreadCounts>();
    return new WebEngineHost(
        services.GetRequiredService<IJSRuntime>(),
        services.GetRequiredService<LocalStorageBookStore>(),
        services.GetRequiredService<StartupBook>().Book,
        threads,
        // The worker starts with the saved setting's threads, so the first search finds the right engine loaded.
        services.GetRequiredService<ISettingsStore>().Load().Game.ThreadsFor(threads));
});
builder.Services.AddSingleton(services => new MainViewModel(
    services.GetRequiredService<IEngineHost>(),
    services.GetRequiredService<IDialogService>(),
    services.GetRequiredService<IGameFileService>(),
    services.GetRequiredService<ISettingsStore>(),
    services.GetRequiredService<StartupBook>().Notice,
    services.GetRequiredService<ISoundService>()));

WebAssemblyHost host = builder.Build();

// The engine host and the view model need the book, so it is loaded before the first page is shown.
await host.Services.GetRequiredService<StartupBook>().LoadAsync();
await host.RunAsync();
