// Web Worker that runs the engine: Stello's Rust engine compiled to WebAssembly (Stello.Rust/stello-wasm, built into
// ../engine by Stello.Web's build). The page looks up the opening book itself and sends only searches.
//
// A search runs to the end inside search(), so this worker cannot receive a message while it runs. The page stops it
// through the SharedArrayBuffer it sends with 'init' instead (1 = Move Now, 2 = cancel), which the engine reads with
// signal() every 1024 nodes.
import load, { init, search, version } from '../engine/stello_wasm.js';

let signalView = null;

const host = {
    progress: json => self.postMessage({ type: 'progress', json }),
    signal: () => (signalView ? Atomics.load(signalView, 0) : 0),
};

const started = load({ module_or_path: new URL('../engine/stello_wasm_bg.wasm', import.meta.url) });

self.addEventListener('message', async e => {
    const { id, command, args, signalBuffer } = e.data;
    if (signalBuffer) {
        signalView = new Int32Array(signalBuffer);
    }
    try {
        await started;
        let result;
        switch (command) {
            // args: hash bits. The built-in evaluation is part of the WebAssembly module.
            case 'init': init(undefined, args[0]); result = version(); break;
            // args: the JSON of a MoveRequest; returns the JSON of a MoveResponse (EngineProtocol).
            case 'search': result = search(args[0], host); break;
            default: throw new Error(`Unknown engine command: ${command}`);
        }
        self.postMessage({ type: 'response', id, result });
    } catch (error) {
        self.postMessage({ type: 'response', id, error: error?.message ?? String(error) });
    }
});
