import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const root = process.cwd();
const wasmModulePath = pathToFileURL(path.join(root, 'build-wasm', 'rangemaster_dsp.js')).href;
const renderExe = path.join(root, 'build', 'rangemaster-render.exe');

const { default: createRangemasterModule } = await import(wasmModulePath);
const Module = await createRangemasterModule();

function readFloat32Wav(wavPath) {
    const buf = fs.readFileSync(wavPath);
    let pos = 12;
    while (pos + 8 <= buf.length) {
        const id = buf.toString('ascii', pos, pos + 4);
        const size = buf.readUInt32LE(pos + 4);
        if (id === 'data') {
            const count = size / 4;
            const out = new Float64Array(count);
            for (let i = 0; i < count; ++i) {
                out[i] = buf.readFloatLE(pos + 8 + i * 4);
            }
            return out;
        }
        pos += 8 + size + (size & 1);
    }
    throw new Error('Missing data chunk in WAV');
}

const engines = [
    { name: 'em-dc', id: 0 },
    { name: 'em-ac', id: 1 },
    { name: 'gp-dc', id: 2 },
    { name: 'gp-ac', id: 3 }
];

const fsHz = 48000;
const nSamples = 4800; // 0.1 s
const freq = 500.0;
const tmpDir = os.tmpdir();

let maxOverallDiff = 0.0;
for (const eng of engines) {
    const nativeWav = path.join(tmpDir, `native_${eng.name}.wav`);
    execFileSync(renderExe, [
        '--gen', 'sine',
        '--freq', String(freq),
        '--duration', '0.1',
        '--sample-rate', String(fsHz),
        '--amp', '1.0',
        '--input-volts', '0.5',
        '--output-ref-volts', '9.0',
        '--engine', eng.name,
        '--transistor', 'oc44-02',
        '--oversampling', '8',
        '--bit-depth', 'float32',
        '--output', nativeWav
    ]);
    const nativeSamples = readFloat32Wav(nativeWav);

    const h = Module._rangemaster_create();
    Module._rangemaster_set_engine(h, eng.id);
    Module._rangemaster_set_transistor(h, 1); // oc44-02
    Module._rangemaster_set_oversampling(h, 8);
    Module._rangemaster_set_input_volts(h, 0.5);
    Module._rangemaster_set_output_ref_volts(h, 9.0);
    const prepOk = Module._rangemaster_prepare(h, fsHz, 256);
    if (prepOk !== 1) throw new Error(`WASM prepare failed for ${eng.name}`);

    const inPtr = Module._malloc(nSamples * 8);
    const outPtr = Module._malloc(nSamples * 8);
    const inView = new Float64Array(Module.HEAPF64.buffer, inPtr, nSamples);
    for (let i = 0; i < nSamples; ++i) {
        inView[i] = Math.sin(2.0 * Math.PI * freq * i / fsHz);
    }
    Module._rangemaster_process_f64(h, inPtr, outPtr, nSamples);
    const wasmSamples = new Float64Array(Module.HEAPF64.buffer, outPtr, nSamples);

    let maxDiff = 0.0;
    for (let i = 0; i < nSamples; ++i) {
        const d = Math.abs(wasmSamples[i] - nativeSamples[i]);
        if (d > maxDiff) maxDiff = d;
        if (!Number.isFinite(wasmSamples[i])) throw new Error(`Non-finite WASM sample at ${i}`);
    }
    maxOverallDiff = Math.max(maxOverallDiff, maxDiff);
    console.log(`WASM vs Native [${eng.name} @ 8x]: max_abs_diff = ${maxDiff.toExponential(3)} (FS units; float32 WAV quantization floor ~1e-7)`);

    Module._free(inPtr);
    Module._free(outPtr);
    Module._rangemaster_destroy(h);

    if (maxDiff > 1e-5) {
        throw new Error(`Native/WASM equivalence exceeded 1e-5 tolerance for ${eng.name}: ${maxDiff}`);
    }
}

console.log(`ALL WASM EQUIVALENCE CHECKS PASSED (maxOverallDiff=${maxOverallDiff.toExponential(3)})`);
