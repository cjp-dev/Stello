// A pool thread of the threaded engine under Node (worker_threads), as engine-pool-worker.js is in the browser: it
// instantiates the module on the shared memory it is given and then runs the engine's thread bodies for ever.
import { workerData } from 'node:worker_threads';

const engine = await import(workerData.glue);
engine.initSync({ module: workerData.module, memory: workerData.memory });
engine.poolWorker();
