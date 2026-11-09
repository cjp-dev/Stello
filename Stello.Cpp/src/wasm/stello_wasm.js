const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();

let wasm = null;
let wasmMemory = null;
let currentHost = null;

function memoryView(ptr, len) {
    return new Uint8Array(wasmMemory.buffer, ptr, len);
}

function readUtf8(ptr, len) {
    if (len === 0) {
        return '';
    }
    const view = memoryView(ptr, len);
    // TextDecoder rejects shared ArrayBuffer views; copy out first when memory is shared.
    return textDecoder.decode(wasmMemory.buffer instanceof ArrayBuffer ? view : view.slice());
}

function readCString(ptr) {
    const bytes = new Uint8Array(wasmMemory.buffer);
    let end = ptr;
    while (bytes[end] !== 0) {
        ++end;
    }
    return readUtf8(ptr, end - ptr);
}

function readResultString() {
    return readUtf8(wasm.stello_wasm_result_ptr(), wasm.stello_wasm_result_len());
}

function writeBytes(bytes) {
    const ptr = wasm.stello_wasm_alloc(bytes.length);
    if (ptr === 0) {
        throw new Error('Out of WebAssembly memory.');
    }
    memoryView(ptr, bytes.length).set(bytes);
    return ptr;
}

function makeImports(module, memory) {
    const ref = { exports: null };
    const env = {
        stello_js_progress(ptr, len) {
            if (currentHost) {
                currentHost.progress(readUtf8(ptr, len));
            }
        },
        stello_js_signal() {
            return currentHost ? (currentHost.signal() | 0) : 0;
        },
        stello_js_absolute_now_ms() {
            return performance.timeOrigin + performance.now();
        },
        stello_js_performance_now_ms() {
            return performance.now();
        },
        emscripten_notify_memory_growth() {},
    };

    const importsInfo = WebAssembly.Module.imports(module);
    const hasMemoryImport = importsInfo.some(i => i.module === 'env' && i.name === 'memory');
    if (hasMemoryImport) {
        Object.assign(env, {
            memory,
            __pthread_create_js() {
                return 0;
            },
            _emscripten_notify_mailbox_postmessage() {},
            emscripten_check_blocking_allowed() {},
            _emscripten_receive_on_main_thread_js() {
                return 0;
            },
            _emscripten_init_main_thread_js(tb) {
                let canBlock = 0;
                try {
                    Atomics.wait(new Int32Array(memory.buffer), 0, 0, 0);
                    canBlock = 1;
                } catch {
                    canBlock = 0;
                }
                ref.exports._emscripten_thread_init(tb, 0, 1, canBlock, 65536, 0);
                ref.exports._emscripten_tls_init();
            },
            _emscripten_thread_mailbox_await() {},
            _emscripten_thread_set_strongref() {},
            emscripten_exit_with_live_runtime() {},
            _emscripten_thread_cleanup() {},
        });
    }

    const wasi_snapshot_preview1 = {
        clock_time_get(clockId, _precision, resultPtr) {
            const ms = clockId === 1 ? performance.now() : (performance.timeOrigin + performance.now());
            const ns = BigInt(Math.round(Math.max(0, ms) * 1e6));
            new DataView(wasmMemory.buffer).setBigUint64(resultPtr, ns, true);
            return 0;
        },
        environ_sizes_get(countPtr, bufSizePtr) {
            const view = new DataView(wasmMemory.buffer);
            view.setUint32(countPtr, 0, true);
            view.setUint32(bufSizePtr, 0, true);
            return 0;
        },
        environ_get() {
            return 0;
        },
        fd_write(_fd, iovsPtr, iovsLen, nwrittenPtr) {
            const view = new DataView(wasmMemory.buffer);
            let total = 0;
            for (let i = 0; i < iovsLen; ++i) {
                total += view.getUint32(iovsPtr + i * 8 + 4, true);
            }
            view.setUint32(nwrittenPtr, total, true);
            return 0;
        },
        fd_seek() {
            return 70; // WASI ESPIPE
        },
        fd_close() {
            return 0;
        },
        proc_exit(code) {
            throw new Error(`WebAssembly exited with code ${code}.`);
        },
    };

    return { imports: { env, wasi_snapshot_preview1 }, ref, hasMemoryImport };
}

function finalizeInstance(instance, sharedMemory, isWorker) {
    const exports = instance.exports;
    wasm = exports;
    wasmMemory = exports.memory || sharedMemory;

    if (isWorker) {
        // Worker threads share linear memory; give each its own C stack, TLS, and pthread block under boot_lock.
        const i32 = new Int32Array(wasmMemory.buffer);
        const lockIdx = exports.stello_wasm_boot_lock_ptr() >> 2;
        while (Atomics.compareExchange(i32, lockIdx, 0, 1) !== 0) {
            Atomics.wait(i32, lockIdx, 1);
        }
        try {
            exports._emscripten_stack_restore(exports.stello_wasm_boot_stack_top());
            const newSp = exports.stello_wasm_worker_prepare();
            exports._emscripten_stack_restore(newSp);
        } finally {
            Atomics.store(i32, lockIdx, 0);
            Atomics.notify(i32, lockIdx, 1);
        }
    } else {
        if (typeof exports._initialize === 'function') {
            exports._initialize();
        }
        if (typeof exports.stello_wasm_record_main === 'function') {
            exports.stello_wasm_record_main();
        }
    }

    return { ...exports, memory: wasmMemory };
}

function ensureSharedMemory(module, maybeMemory) {
    if (maybeMemory) {
        return { memory: maybeMemory, isWorker: true };
    }
    const importsInfo = WebAssembly.Module.imports(module);
    const hasMemoryImport = importsInfo.some(i => i.module === 'env' && i.name === 'memory');
    if (!hasMemoryImport) {
        return { memory: null, isWorker: false };
    }
    // Initial 32 MiB (512 pages), max 2 GiB (32768 pages) matching HashBits settings.
    const memory = new WebAssembly.Memory({ initial: 512, maximum: 32768, shared: true });
    return { memory, isWorker: false };
}

export function initSync(moduleOrOptions, maybeMemory) {
    let module = moduleOrOptions;
    let memory = maybeMemory;
    if (module !== undefined && !(module instanceof WebAssembly.Module) && !ArrayBuffer.isView(module) && !(module instanceof ArrayBuffer)) {
        memory = module.memory;
        module = module.module;
    }
    if (!(module instanceof WebAssembly.Module)) {
        module = new WebAssembly.Module(module);
    }

    const memInfo = ensureSharedMemory(module, memory);
    const { imports, ref } = makeImports(module, memInfo.memory);
    if (memInfo.memory) {
        wasmMemory = memInfo.memory;
    }
    const instance = new WebAssembly.Instance(module, imports);
    ref.exports = instance.exports;
    return finalizeInstance(instance, memInfo.memory, memInfo.isWorker);
}

async function resolveModule(moduleOrPath) {
    if (moduleOrPath instanceof WebAssembly.Module) {
        return moduleOrPath;
    }
    if (ArrayBuffer.isView(moduleOrPath) || moduleOrPath instanceof ArrayBuffer) {
        return WebAssembly.compile(moduleOrPath);
    }
    const response = (typeof Response !== 'undefined' && moduleOrPath instanceof Response)
        ? moduleOrPath
        : await fetch(moduleOrPath);
    if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
    }
    const bytes = await response.arrayBuffer();
    return WebAssembly.compile(bytes);
}

export default async function initWasm(moduleOrPath) {
    let target = moduleOrPath;
    let maybeMemory;
    if (target !== undefined && typeof target === 'object' && target !== null && !(target instanceof URL)
        && !(typeof Request !== 'undefined' && target instanceof Request)
        && !(typeof Response !== 'undefined' && target instanceof Response)
        && !(target instanceof WebAssembly.Module) && !(target instanceof ArrayBuffer) && !ArrayBuffer.isView(target)) {
        maybeMemory = target.memory;
        target = target.module_or_path;
    }
    if (target === undefined) {
        target = new URL('./stello_wasm_bg.wasm', import.meta.url);
    }

    const module = await resolveModule(target);
    const memInfo = ensureSharedMemory(module, maybeMemory);
    const { imports, ref } = makeImports(module, memInfo.memory);
    if (memInfo.memory) {
        wasmMemory = memInfo.memory;
    }
    const instance = await WebAssembly.instantiate(module, imports);
    ref.exports = instance.exports;
    return finalizeInstance(instance, memInfo.memory, memInfo.isWorker);
}

export function version() {
    return readCString(wasm.stello_wasm_version());
}

export function init(evaluation, hashBits) {
    const bits = hashBits >>> 0;
    if (evaluation === undefined || evaluation === null) {
        const rc = wasm.stello_wasm_init(0, 0, 0, bits);
        if (rc !== 0) {
            throw new Error(readResultString());
        }
        return;
    }

    const bytes = evaluation instanceof Uint8Array ? evaluation : new Uint8Array(evaluation);
    const ptr = writeBytes(bytes);
    try {
        const rc = wasm.stello_wasm_init(1, ptr, bytes.length, bits);
        if (rc !== 0) {
            throw new Error(readResultString());
        }
    } finally {
        wasm.stello_wasm_free(ptr);
    }
}

export function search(requestJson, host) {
    const bytes = textEncoder.encode(requestJson);
    const ptr = writeBytes(bytes);
    const previousHost = currentHost;
    currentHost = host ?? null;
    try {
        const rc = wasm.stello_wasm_search(ptr, bytes.length);
        if (rc === 0) {
            return readResultString();
        }
        if (rc === 1) {
            throw new Error('cancelled');
        }
        throw new Error(readResultString());
    } finally {
        currentHost = previousHost;
        wasm.stello_wasm_free(ptr);
    }
}

export function threaded() {
    return wasm.stello_wasm_threaded() !== 0;
}

export function idleThreads() {
    return wasm.stello_wasm_idle_threads() >>> 0;
}

export function poolWorker() {
    wasm.stello_wasm_pool_worker();
}
