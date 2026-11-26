import * as esbuild from 'esbuild';
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';

const webRoot = process.cwd();
const generated = path.join(webRoot, 'src', 'wasm');
const jsPath = path.join(generated, 'rangemaster_dsp.js');
const wasmPath = path.join(webRoot, 'public', 'wasm', 'rangemaster_dsp.wasm');
if (!fs.existsSync(jsPath) || !fs.existsSync(wasmPath)) throw new Error('Build WASM from C++ first: scripts/build-wasm');
// Refuse stale checked-in binaries after any ABI/core source change.
const sources = execFileSync('git', ['ls-files', '../src', '../CMakeLists.txt'], { cwd: webRoot, encoding: 'utf8' }).trim().split(/\r?\n/);
const hash = createHash('sha256');
for (const source of sources.sort()) hash.update(source).update(fs.readFileSync(path.resolve(webRoot, source), 'utf8').replace(/\r\n/g, '\n'));
const sourceHash = hash.digest('hex');
const stampPath = path.join(generated, 'source.sha256');
if (!fs.existsSync(stampPath) || fs.readFileSync(stampPath, 'utf8').trim() !== sourceHash) throw new Error('Stale WASM: rebuild scripts/build-wasm before web build');
const pair = JSON.parse(fs.readFileSync(path.join(generated, 'artifact-hashes.json'), 'utf8'));
const digest = p => createHash('sha256').update(fs.readFileSync(p)).digest('hex');
if (pair.js !== digest(jsPath) || pair.wasm !== digest(wasmPath)) throw new Error('Mismatched JS/WASM pair; rebuild WASM');
const webSources = execFileSync('git', ['ls-files', 'src', 'scripts', 'vite.config.ts', 'package.json', 'package-lock.json'], { cwd: webRoot, encoding: 'utf8' }).trim().split(/\r?\n/).filter(p => !p.includes('rangemaster_dsp.js'));
const webHash = createHash('sha256');
for (const file of webSources.sort()) if (fs.existsSync(path.join(webRoot,file))) webHash.update(file).update(fs.readFileSync(path.join(webRoot,file), 'utf8').replace(/\r\n/g, '\n'));
const coreVersion = fs.readFileSync(path.join(webRoot, '..', 'CMakeLists.txt'), 'utf8').match(/project\(rangemaster_dsp VERSION ([0-9.]+)/)?.[1];
if (!coreVersion) throw new Error('Missing CMake core version');
const build = { core_version: coreVersion, milestone: 'M7.1', web_source_sha256: webHash.digest('hex'), emscripten: pair.emscripten, commit: execFileSync('git', ['rev-parse', 'HEAD'], { encoding: 'utf8' }).trim(),
  dirty: !!execFileSync('git', ['status', '--porcelain', '--untracked-files=no'], { encoding: 'utf8' }).trim(),
  source_sha256: sourceHash, wasm_sha256: createHash('sha256').update(fs.readFileSync(wasmPath)).digest('hex'), abi: 1 };
fs.writeFileSync(path.join(generated, 'build-info.ts'), `export const BUILD_INFO = ${JSON.stringify(build)} as const;\n`);
const outDir = path.join(webRoot, 'public', 'worklet');
fs.mkdirSync(outDir, { recursive: true });

await esbuild.build({
  entryPoints: [path.join(webRoot, 'src', 'worklet', 'rangemaster-processor.ts')],
  bundle: true,
  format: 'esm',
  platform: 'browser',
  target: ['es2022'],
  outfile: path.join(outDir, 'rangemaster-processor.js'),
  external: ['module', 'fs', 'path', 'url'],
  minify: false,
  sourcemap: false,
  banner: {
    js: `if (typeof globalThis.URL === "undefined") { globalThis.URL = class URL { constructor(u, b) { this.href = (b ? String(b) : "") + String(u); } }; }`,
  },
  define: {
    'process.env.NODE_ENV': '"production"',
  },
});

console.log('Built public/worklet/rangemaster-processor.js');
