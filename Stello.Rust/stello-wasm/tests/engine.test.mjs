// The WebAssembly engine as the web app's worker uses it (wasm-bindgen --target web), under Node:
//   node --test Stello.Rust/stello-wasm/tests/engine.test.mjs
// It loads the package that Stello.Web's build makes (Stello.Net/Stello.Web/wwwroot/engine), or the folder in the
// environment variable STELLO_WASM_PKG. The searches must equal Stello 2.0's recorded signatures node for node, as
// the native engine does (stello-core/tests/search.rs).
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';

const repository = fileURLToPath(new URL('../../../', import.meta.url));
const pkg = process.env.STELLO_WASM_PKG ?? `${repository}Stello.Net/Stello.Web/wwwroot/engine`;
const engine = await import(pathToFileURL(`${pkg}/stello_wasm.js`).href);
engine.initSync({ module: readFileSync(`${pkg}/stello_wasm_bg.wasm`) });
engine.init(undefined, 19);

// <id> <64 squares, a1 first> <X|O> lines of a benchmark position file.
function positions(file) {
    const text = readFileSync(`${repository}Stello.Net/tools/Stello.Benchmark/Data/${file}`, 'utf8');
    const result = new Map();
    for (const line of text.split('\n')) {
        if (line.trim() === '' || line.startsWith('#')) {
            continue;
        }
        const [id, squares, side] = line.split(';')[0].trim().split(/\s+/);
        let black = 0n;
        let white = 0n;
        [...squares].forEach((c, i) => {
            if (c === 'X') black |= 1n << BigInt(i);
            if (c === 'O') white |= 1n << BigInt(i);
        });
        result.set(id, { black, white, player: side === 'X' ? 0 : 1 });
    }
    return result;
}

// As EngineProtocol.WriteRequest writes it (System.Text.Json): the boards are 64-bit integers.
function request({ black, white, player }, { mode = 0, depth = 0, timeMs = 0, hashBits = 19 } = {}) {
    return `{"Black":${black},"White":${white},"Player":${player},"Mode":${mode},"Depth":${depth},` +
        `"TimeTicks":${timeMs * 10000},"UseOpeningBook":true,"HashBits":${hashBits}}`;
}

const kinds = ['None', 'Heuristic', 'WinLossDraw', 'Exact', 'Book'];
const squareName = square => square < 0 ? 'pass' : String.fromCharCode(97 + square % 8) + (Math.floor(square / 8) + 1);
const run = (text, host = { progress() {}, signal: () => 0 }) => JSON.parse(engine.search(text, host));
const signatures = readFileSync(`${repository}Stello.Net/Stello.Engine.Tests/Data/stello-v2-signatures.txt`, 'utf8')
    .split('\n').filter(line => line !== '' && !line.startsWith('#'));

function checkSignature(line, files) {
    const [file, id, limit] = line.split(' ');
    const limits = limit === 'solve' ? { mode: 3 } : { mode: 0, depth: Number(limit.split('=')[1]) };
    // StelloV2Tests uses a new engine for each search.
    engine.init(undefined, 19);
    const result = run(request(files[file].get(id), limits));
    const actual = [file, id, limit, squareName(result.Square), result.Score, kinds[result.Kind], result.Depth, result.Nodes];
    assert.equal(actual.join(' '), line);
}

test('the version is 3.x', () => {
    assert.match(engine.version(), /^3\.\d+\.\d+$/);
});

test('midgame searches equal Stello 2.0 node for node', () => {
    const files = { 'midgame.txt': positions('midgame.txt') };
    const lines = signatures.filter(line => line.startsWith('midgame.txt ')).filter((_, i) => i % 5 === 0);
    assert.equal(lines.length, 20);
    lines.forEach(line => checkSignature(line, files));
});

test('solves equal Stello 2.0 node for node (FFO #40 and #41)', () => {
    const files = { 'ffo-40-59.txt': positions('ffo-40-59.txt') };
    for (const line of signatures.filter(line => / (40|41) solve /.test(line))) {
        checkSignature(line, files);
    }
});

test('progress is posted for new depths and best moves, otherwise at most every 100 ms', () => {
    engine.init(undefined, 19);
    const reports = [];
    const start = performance.now();
    run(request(positions('midgame.txt').get('g01e36'), { mode: 1, timeMs: 600 }), {
        progress: json => reports.push({ at: performance.now() - start, ...JSON.parse(json) }),
        signal: () => 0,
    });
    assert.ok(reports.length > 3, `${reports.length} reports`);
    for (let i = 1; i < reports.length; i++) {
        const [last, report] = [reports[i - 1], reports[i]];
        const changed = report.Depth !== last.Depth || report.BestMove !== last.BestMove;
        // A little slack: the engine and this test read the clock at slightly different moments.
        assert.ok(changed || report.at - last.at >= 99, `report ${i} after ${report.at - last.at} ms`);
        assert.ok(report.Nodes >= last.Nodes);
    }
    assert.ok(reports.every(report => report.Kind === 1 && report.CurrentMove >= 0 && report.CurrentMove < 64));
});

test('a time per move is kept with the performance.now clock', () => {
    engine.init(undefined, 19);
    const start = performance.now();
    const result = run(request(positions('midgame.txt').get('g01e36'), { mode: 1, timeMs: 300 }));
    const elapsed = performance.now() - start;
    assert.ok(elapsed < 450, `${elapsed} ms`);
    assert.ok(result.ElapsedTicks > 0 && result.ElapsedTicks / 10000 <= elapsed + 1);
});

// The page sets the signal while the search runs; here it is set when a deadline passes.
function stopAfter(ms, code) {
    const deadline = performance.now() + ms;
    return { progress() {}, signal: () => (performance.now() >= deadline ? code : 0) };
}

test('Move Now returns a legal move at once, in a midgame search and in a solve', () => {
    const ffo = positions('ffo-40-59.txt');
    for (const [position, limits] of [[positions('midgame.txt').get('g01e36'), { mode: 0, depth: 30 }], [ffo.get('45'), { mode: 3 }]]) {
        engine.init(undefined, 19);
        const host = stopAfter(200, 1);
        const start = performance.now();
        const result = run(request(position, limits), host);
        const latency = performance.now() - start - 200;
        assert.ok(latency < 20, `Move Now took ${latency} ms`);
        assert.ok(result.Square >= 0);
        const own = position.player === 0 ? position.black : position.white;
        assert.equal((own | (position.player === 0 ? position.white : position.black)) & (1n << BigInt(result.Square)), 0n);
    }
});

test('cancel throws "cancelled" at once, and the engine searches again afterwards', () => {
    engine.init(undefined, 19);
    const position = positions('midgame.txt').get('g01e36');
    const start = performance.now();
    assert.throws(() => run(request(position, { mode: 0, depth: 30 }), stopAfter(100, 2)), /cancelled/);
    assert.ok(performance.now() - start - 100 < 20);
    assert.equal(run(request(position, { mode: 0, depth: 4 })).Depth, 4);
});

test('other hash bits make a new engine; requests that are not valid throw', () => {
    engine.init(undefined, 19);
    const position = positions('midgame.txt').get('g01e48');
    const small = run(request(position, { depth: 10, hashBits: 12 }));
    const again = run(request(position, { depth: 10, hashBits: 19 }));
    assert.equal(again.Nodes, 37483);
    assert.ok(small.Nodes > 0);
    assert.throws(() => engine.search('{"Black":1}', { progress() {}, signal: () => 0 }), /no White/);
    assert.throws(() => engine.search(request(position, { depth: 61 }), { progress() {}, signal: () => 0 }), /not valid/);
    assert.throws(() => engine.init(new Uint8Array([1, 2, 3]), 19), /gzip|evaluation/i);
    assert.throws(() => engine.init(undefined, 9), /10-26/);
});

test('the evaluation file passed as bytes searches like the built-in one', () => {
    engine.init(readFileSync(`${repository}Stello.Net/Book/stello-eval.bin`), 19);
    const result = run(request(positions('midgame.txt').get('g01e48'), { depth: 10 }));
    assert.equal(result.Nodes, 37483);
    engine.init(undefined, 19);
});
