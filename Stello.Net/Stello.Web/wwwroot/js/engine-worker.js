// Web Worker that runs the engine in its own .NET runtime, loaded from the app's _framework folder.
// The page passes the (fingerprinted) URL of dotnet.js in the query string.
const dotnetUrl = new URL(self.location.href).searchParams.get('dotnet');

const started = (async () => {
    const { dotnet } = await import(dotnetUrl);
    const runtime = await dotnet.create();
    runtime.setModuleImports('engine-worker', {
        postProgress: json => self.postMessage({ type: 'progress', json }),
    });
    const exports = await runtime.getAssemblyExports(runtime.getConfig().mainAssemblyName);
    return exports.Stello.Web.Worker.EngineWorker;
})();

self.addEventListener('message', async e => {
    const { id, command, args } = e.data;
    try {
        const engine = await started;
        let result;
        switch (command) {
            case 'init': result = engine.Init(...args); break;
            case 'chooseMove': result = engine.ChooseMove(...args); break;
            case 'resetBookTracker': result = engine.ResetBookTracker(); break;
            case 'addGame': result = engine.AddGame(...args); break;
            case 'getBook': result = engine.GetBook(); break;
            default: throw new Error(`Unknown engine command: ${command}`);
        }
        self.postMessage({ type: 'response', id, result });
    } catch (error) {
        self.postMessage({ type: 'response', id, error: error?.message ?? String(error) });
    }
});
