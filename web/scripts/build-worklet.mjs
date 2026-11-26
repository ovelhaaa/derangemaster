import * as esbuild from 'esbuild';
import fs from 'node:fs';
import path from 'node:path';

const webRoot = process.cwd();
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
  define: {
    'process.env.NODE_ENV': '"production"',
  },
});

console.log('Built public/worklet/rangemaster-processor.js');
