// The threaded WebAssembly engine (Plans/making-stello-3-plan.md, phase 8) under Node:
//   node --test Stello.Rust/stello-wasm/tests/engine-mt.test.mjs
// It loads the package that Stello.Web's build makes with the pinned nightly (Stello.Net/Stello.Web/wwwroot/engine-mt),
// or the folder in STELLO_WASM_MT_PKG, and skips if there is none. The pool threads are worker_threads running
// pool-worker.mjs on the shared memory, as engine-pool-worker.js does in the browser. With 1 thread the searches equal
// the baseline signatures node for node; with more, solves are exact and moves legal.
import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import { test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { Worker } from 'node:worker_threads';

const repository = fileURLToPath(new URL('../../../', import.meta.url));
const pkg = process.env.STELLO_WASM_MT_PKG ?? `${repository}Stello.Net/Stello.Web/wwwroot/engine-mt`;
const available = existsSync(`${pkg}/stello_wasm_bg.wasm`);
const THREADS = 4;

let engine;
if (available) {
    const glue = pathToFileURL(`${pkg}/stello_wasm.js`).href;
    engine = await import(glue);
    const module = new WebAssembly.Module(readFileSync(`${pkg}/stello_wasm_bg.wasm`));
    const { memory } = engine.initSync({ module });
    for (let i = 0; i < THREADS - 1; i++) {
        // unref: the workers wait for ever, and must not keep the test process alive.
        new Worker(new URL('./pool-worker.mjs', import.meta.url), { workerData: { module, memory, glue } }).unref();
    }
    while (engine.idleThreads() < THREADS - 1) {
        await new Promise(resolve => setTimeout(resolve, 5));
    }
}

const options = { skip: available ? false : `no threaded package in ${pkg}` };

function positions(file) {
    const text = readFileSync(`${repository}Stello.Net/tools/Stello.Benchmark/Data/${file}`, 'utf8');
    const result = new Map();
    for (const line of text.split('\n')) {
        if (line.trim() === '' || line.startsWith('#')) {
            continue;
        }
        const [head, ...moves] = line.split(';');
        const [id, squares, side] = head.trim().split(/\s+/);
        let black = 0n;
        let white = 0n;
        [...squares].forEach((c, i) => {
            if (c === 'X') black |= 1n << BigInt(i);
            if (c === 'O') white |= 1n << BigInt(i);
        });
        const scores = moves.map(m => Number(m.split(':')[1])).filter(Number.isFinite);
        result.set(id, { black, white, player: side === 'X' ? 0 : 1, best: scores.length ? Math.max(...scores) : null });
    }
    return result;
}

function request({ black, white, player }, { mode = 0, depth = 0, timeMs = 0, threads = 1 } = {}) {
    return `{"Black":${black},"White":${white},"Player":${player},"Mode":${mode},"Depth":${depth},` +
        `"TimeTicks":${timeMs * 10000},"UseOpeningBook":true,"HashBits":19,"Threads":${threads}}`;
}

const run = (text, host = { progress() {}, signal: () => 0 }) => JSON.parse(engine.search(text, host));
const kinds = ['None', 'Heuristic', 'WinLossDraw', 'Exact', 'Book'];
const squareName = square => square < 0 ? 'pass' : String.fromCharCode(97 + square % 8) + (Math.floor(square / 8) + 1);
const legal = (position, square) => {
    const occupied = position.black | position.white;
    return square >= 0 && (occupied & (1n << BigInt(square))) === 0n;
};

test('this is the threaded build', options, () => {
    assert.equal(engine.threaded(), true);
    assert.equal(engine.idleThreads(), THREADS - 1);
});

test('with 1 thread, searches equal the baseline node for node', options, () => {
    const signatures = readFileSync(`${repository}Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt`, 'utf8')
        .split(/\r?\n/).filter(line => line !== '' && !line.startsWith('#'));
    const files = { 'midgame.txt': positions('midgame.txt'), 'ffo-40-59.txt': positions('ffo-40-59.txt') };
    const lines = signatures.filter(line => line.startsWith('midgame.txt ')).filter((_, i) => i % 10 === 0)
        .concat(signatures.filter(line => / (40|41) solve /.test(line)));
    for (const line of lines) {
        const [file, id, limit] = line.split(' ');
        const limits = limit === 'solve' ? { mode: 3 } : { mode: 0, depth: Number(limit.split('=')[1]) };
        engine.init(undefined, 19);
        const result = run(request(files[file].get(id), limits));
        const actual = [file, id, limit, squareName(result.Square), result.Score, kinds[result.Kind], result.Depth, result.Nodes];
        assert.equal(actual.join(' '), line);
    }
});

test(`with ${THREADS} threads, FFO #40-#44 are solved exactly`, options, () => {
    const ffo = positions('ffo-40-59.txt');
    for (const id of ['40', '41', '42', '43', '44']) {
        engine.init(undefined, 19);
        const result = run(request(ffo.get(id), { mode: 3, threads: THREADS }));
        assert.equal(result.Score, ffo.get(id).best, `#${id}`);
        assert.equal(kinds[result.Kind], 'Exact');
        assert.ok(legal(ffo.get(id), result.Square));
    }
});

test(`with ${THREADS} threads, midgame searches reach their depth with a legal move`, options, () => {
    const midgame = positions('midgame.txt');
    for (const id of ['g01e48', 'g01e36', 'g01e26']) {
        engine.init(undefined, 19);
        const result = run(request(midgame.get(id), { depth: 12, threads: THREADS }));
        assert.equal(result.Depth, 12);
        assert.ok(legal(midgame.get(id), result.Square), id);
    }
});

test(`with ${THREADS} threads, Move Now and cancel answer at once`, options, () => {
    const ffo = positions('ffo-40-59.txt');
    for (const code of [1, 2]) {
        engine.init(undefined, 19);
        const deadline = performance.now() + 200;
        const host = { progress() {}, signal: () => (performance.now() >= deadline ? code : 0) };
        const start = performance.now();
        if (code === 1) {
            const result = run(request(ffo.get('45'), { mode: 3, threads: THREADS }), host);
            assert.ok(legal(ffo.get('45'), result.Square));
        } else {
            assert.throws(() => run(request(ffo.get('45'), { mode: 3, threads: THREADS }), host), /cancelled/);
        }
        const latency = performance.now() - start - 200;
        assert.ok(latency < 30, `${code === 1 ? 'Move Now' : 'cancel'} took ${latency} ms`);
    }
    engine.init(undefined, 19);
    assert.equal(run(request(positions('midgame.txt').get('g01e36'), { depth: 6, threads: THREADS })).Depth, 6);
});
