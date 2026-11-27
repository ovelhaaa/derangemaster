import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
const sources = execFileSync('git', ['ls-files', 'src', 'CMakeLists.txt'], { encoding: 'utf8' }).trim().split(/\r?\n/);
const hash = createHash('sha256');
for (const source of sources.sort()) hash.update('../' + source).update(fs.readFileSync(source, 'utf8').replace(/\r\n/g, '\n'));
fs.writeFileSync('web/src/wasm/source.sha256', hash.digest('hex') + '\n');

const glue = 'web/src/wasm/rangemaster_dsp.js';
fs.writeFileSync(glue, fs.readFileSync(glue, 'utf8').replace('new URL("rangemaster_dsp.wasm",import.meta.url)', 'new URL(/* @vite-ignore */"rangemaster_dsp.wasm",import.meta.url)'));
const digest = p => createHash('sha256').update(fs.readFileSync(p)).digest('hex');
fs.writeFileSync('web/src/wasm/artifact-hashes.json', JSON.stringify({ emscripten: fs.readFileSync('build-wasm/emscripten-version.txt', 'utf8').split(/\r?\n/)[0], js: digest(glue), wasm: digest('web/public/wasm/rangemaster_dsp.wasm') }));
