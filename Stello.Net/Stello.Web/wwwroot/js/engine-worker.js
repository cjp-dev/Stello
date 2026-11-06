// Web Worker that runs the engine: Stello's Rust engine compiled to WebAssembly (Stello.Rust/stello-wasm, built by
// Stello.Web's build). The page looks up the opening book itself and sends only searches.
//
// There are two builds of the engine:
// - ../engine-mt: threaded, on a shared memory (Plans/making-stello-3-plan.md, phase 8). This worker is the search's
//   main thread, and the other threads are pool workers (engine-pool-worker.js) that this worker starts and that run
//   on the same memory. Used when the page asks for more than 1 thread and can share memory (cross-origin isolated);
// - ../engine: single-threaded, otherwise, and when the threaded build cannot be loaded.
// Only the build that is used is downloaded.
//
// A search runs to the end inside search(), so this worker cannot receive a message while it runs. The page stops it
// through the SharedArrayBuffer it sends with 'init' instead (1 = Move Now, 2 = cancel), which the engine reads with
// signal() every 1024 nodes and while it waits for its threads.

let signalView = null;
let engine = null;
// 'threaded' or 'single': the build that runs.
let mode = null;
// The threaded build's compiled module and memory, which the pool workers share.
let shared = null;
const poolWorkers = [];
let reportedThreadCount = 0;

const host = {
    progress: json => self.postMessage({ type: 'progress', json }),
    signal: () => (signalView ? Atomics.load(signalView, 0) : 0),
};

const hardwareThreads = Math.max(1, navigator.hardwareConcurrency || 1);
let poolWorkerLimit = Infinity;

function currentInfo() {
    return { version: engine.version(), mode, threads: mode === 'threaded' ? poolWorkers.length + 1 : 1 };
}

// Loads the threaded build if `threads` > 1 and memory can be shared, else (or if that fails) the single-threaded one.
async function load(threads) {
    if (threads > 1 && self.crossOriginIsolated && typeof SharedArrayBuffer !== 'undefined') {
        try {
            const glue = new URL('../engine-mt/stello_wasm.js', import.meta.url);
            const threaded = await import(glue.href);
            const response = await fetch(new URL('../engine-mt/stello_wasm_bg.wasm', import.meta.url));
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }
            const module = await WebAssembly.compile(await response.arrayBuffer());
            const exports = await threaded.default({ module_or_path: module });
            engine = threaded;
            shared = { module, memory: exports.memory, glue: glue.href };
            mode = 'threaded';
            return;
        } catch (error) {
            console.warn('The threaded engine could not be loaded; the single-threaded one plays.', error);
        }
    }

    const single = await import('../engine/stello_wasm.js');
    await single.default({ module_or_path: new URL('../engine/stello_wasm_bg.wasm', import.meta.url) });
    engine = single;
    mode = 'single';
}

function startPoolWorker() {
    let worker;
    return new Promise((resolve, reject) => {
        worker = new Worker(new URL('./engine-pool-worker.js', import.meta.url), { type: 'module' });
        const cleanup = () => {
            worker.removeEventListener('message', onMessage);
            worker.removeEventListener('error', onError);
            worker.removeEventListener('messageerror', onMessageError);
        };
        const onMessage = e => {
            if (e.data === 'ready') {
                cleanup();
                resolve(worker);
            }
        };
        const onError = e => {
            // Handled here: without this the error also reaches this worker's global scope and the page, which then
            // treats the whole engine as failed and restarts it, again and again.
            e.preventDefault();
            cleanup();
            reject(new Error(`A thread of the engine could not start: ${e.message}`));
        };
        const onMessageError = () => {
            cleanup();
            reject(new Error('A thread of the engine could not start: its ready message could not be read.'));
        };
        worker.addEventListener('message', onMessage);
        worker.addEventListener('error', onError, { once: true });
        worker.addEventListener('messageerror', onMessageError, { once: true });
        worker.postMessage(shared);
    }).catch(error => {
        worker?.terminate();
        throw error;
    });
}

// Starts pool workers until there are `count`, retrying each failed startup once.
async function ensurePoolWorkers(count) {
    const idleBefore = engine.idleThreads();
    const workersBefore = poolWorkers.length;
    while (poolWorkers.length < count && poolWorkers.length < poolWorkerLimit) {
        let worker = null;
        for (let attempt = 1; attempt <= 2; attempt++) {
            try {
                worker = await startPoolWorker();
                break;
            } catch (error) {
                console.warn(`Engine pool worker startup attempt ${attempt} failed.`, error);
                if (attempt === 2) {
                    poolWorkerLimit = poolWorkers.length;
                }
            }
        }
        if (!worker) {
            break;
        }
        poolWorkers.push(worker);
    }
    const started = poolWorkers.length - workersBefore;
    // 'ready' comes just before a worker starts to wait in the engine; until then it does not count as idle.
    const target = idleBefore + started;
    const deadline = performance.now() + 2000;
    while (started > 0 && engine.idleThreads() < target && performance.now() < deadline) {
        await new Promise(resolve => setTimeout(resolve, 1));
    }
}

// The pool workers that a search with `threads` threads needs (they are started once and kept).
const poolWorkersFor = threads => Math.max(1, Math.min(threads, hardwareThreads)) - 1;

// The request with its threads capped at what this build and this device can run, and the pool workers they need.
async function prepare(json) {
    const asked = Number(/"Threads":(\d+)/.exec(json)?.[1] ?? 1);
    const desired = mode === 'threaded' ? Math.max(1, Math.min(asked, hardwareThreads)) : 1;
    if (desired > 1) {
        await preparePoolWorkers(desired - 1);
    }
    const availableThreads = poolWorkers.length + 1;
    if (mode === 'threaded' && availableThreads !== reportedThreadCount) {
        reportedThreadCount = availableThreads;
        self.postMessage({ type: 'info', info: currentInfo() });
    }
    const threads = mode === 'threaded' ? Math.min(desired, poolWorkers.length + 1) : 1;
    return threads === asked ? json : json.replace(/"Threads":\d+/, `"Threads":${threads}`);
}

let loaded = null;

// Pool workers are started one request at a time, from 'init' on, so that the first search with threads finds them
// ready.
let preparing = Promise.resolve();
function preparePoolWorkers(count) {
    preparing = preparing.then(
        () => ensurePoolWorkers(count),
        () => ensurePoolWorkers(count),
    );
    return preparing;
}

self.addEventListener('message', async e => {
    const { id, command, args, signalBuffer } = e.data;
    if (signalBuffer) {
        signalView = new Int32Array(signalBuffer);
    }
    try {
        let result;
        switch (command) {
            // args: hash bits and the threads the page will ask for. The built-in evaluation is part of the module.
            case 'init':
                loaded ??= load(args[1] ?? 1);
                await loaded;
                engine.init(undefined, args[0]);
                if (mode === 'threaded') {
                    await preparePoolWorkers(poolWorkersFor(args[1] ?? 1));
                }
                result = currentInfo();
                reportedThreadCount = result.threads;
                break;
            // args: the JSON of a MoveRequest; returns the JSON of a MoveResponse (EngineProtocol).
            case 'search':
                await loaded;
                result = engine.search(await prepare(args[0]), host);
                break;
            default: throw new Error(`Unknown engine command: ${command}`);
        }
        self.postMessage({ type: 'response', id, result });
    } catch (error) {
        self.postMessage({ type: 'response', id, error: error?.message ?? String(error) });
    }
});
