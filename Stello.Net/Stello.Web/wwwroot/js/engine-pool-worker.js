// A thread of the threaded engine (../engine-mt), started by engine-worker.js: it instantiates the engine's module on
// the memory that it shares with engine-worker.js and the other pool workers, says it is ready, and then runs the
// engine's thread bodies for ever (it waits for them with Atomics.wait, which a worker may do). It ends with the
// engine worker.
self.addEventListener('message', async e => {
    const { module, memory, glue } = e.data;
    const engine = await import(glue);
    engine.initSync({ module, memory });
    self.postMessage('ready');
    engine.poolWorker();
}, { once: true });
