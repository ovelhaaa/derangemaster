import { beforeAll, describe, expect, test } from 'vitest';
import fs from 'node:fs';
import { applyFullConfigToHandle, CONFIG_OFFSETS, instantiateRangemasterWasm, writeConfig } from '../src/wasm/rangemasterWasm';
import { NOMINAL_DEFAULT_CONFIG } from '../src/state/presets';
import { computeFileLevelMatchGains, renderWasmSlice } from '../src/audio/LevelMatch';
import { encodeFloat32Wav, exportWavAndManifest } from '../src/audio/OfflineExport';
import type { RangemasterWasmModule } from '../src/wasm/rangemasterWasm';
import { fractionalCoefficients } from '../src/audio/LatencyAlignment';

const wasm = fs.readFileSync('public/wasm/rangemaster_dsp.wasm');
let M: RangemasterWasmModule;
let Processor: any;
class BaseProcessor { port = { messages: [] as any[], onmessage: null, postMessage(msg: unknown) { this.messages.push(structuredClone(msg)); } }; }
beforeAll(async () => {
  M = await instantiateRangemasterWasm(new Uint8Array(wasm).buffer);
  Object.assign(globalThis, { sampleRate: 48000, AudioWorkletProcessor: BaseProcessor, registerProcessor: (_: string, p: any) => { Processor = p; } });
  await import('../src/worklet/rangemaster-processor');
});
const cfg = () => structuredClone(NOMINAL_DEFAULT_CONFIG);
async function processor() {
  const p = new Processor(); await p.initWasm(new Uint8Array(wasm).buffer, cfg(), 'A');
  expect(p.ready).toBe(true); return p;
}
function block(p: any, phase: number, kind = 'sine') {
  const input = new Float32Array(128), out = new Float32Array(128);
  for (let i = 0; i < 128; ++i) {
    const t = phase * 128 + i;
    input[i] = kind === 'impulse' ? (t === 0 ? 0.5 : 0) : kind === 'noise' ? Math.sin(t * 123.4567) * .3 : kind === 'multitone' ? .2 * (Math.sin(t * .02) + Math.sin(t * .131)) : .4 * Math.sin(t * .057);
  }
  p.process([[input]], [[out]]); expect(Array.from(out).every(Number.isFinite)).toBe(true); return out;
}
describe('C ABI on real WASM', () => {
  test('binary layout, rollback and rebuild count', () => {
    expect(CONFIG_OFFSETS).toEqual([0,4,8,12,16,20,24,32,40,48,56,64,72,80,88,96,100]);
    expect(M._rangemaster_get_config_struct_size()).toBe(104);
    const h = M._rangemaster_create(), ptr = M._malloc(104);
    try {
      expect(applyFullConfigToHandle(M,h,cfg()).ok).toBe(true);
      expect(M._rangemaster_prepare(h,48000,128)).toBe(1);
      const before = M._rangemaster_get_matrix_rebuild_count(h);
      const invalid = { ...cfg(), engine: 'em-dc', boost: .3, outputRefVolts: -1 } as any;
      expect(applyFullConfigToHandle(M,h,invalid).error).toContain('outputRefVolts');
      const view = new DataView(M.HEAPU8.buffer,ptr,104); writeConfig(view,cfg());
      expect(M._rangemaster_get_config(h,ptr)).toBe(1);
      expect(view.getInt32(8,true)).toBe(3); expect(view.getFloat64(24,true)).toBe(cfg().boost);
      expect(M._rangemaster_get_matrix_rebuild_count(h)).toBe(before);
      expect(applyFullConfigToHandle(M,h,{...cfg(),inputVolts:.6}).ok).toBe(true);
      expect(M._rangemaster_get_matrix_rebuild_count(h)).toBe(before);
    } finally { M._free(ptr); M._rangemaster_destroy(h); }
  });
  test('invalid imported names are rejected, never replaced by nominal defaults', () => {
    expect(() => writeConfig(new DataView(new ArrayBuffer(104)), {...cfg(),transistorId:'unknown'})).toThrow();
    expect(() => writeConfig(new DataView(new ArrayBuffer(104)), {...cfg(),oversampling:8.7} as any)).toThrow(/integer/);
    expect(() => writeConfig(new DataView(new ArrayBuffer(104)), {...cfg(),gpAblation:{a:true,b:true,c:true,d:true,e:true}} as any)).toThrow(/ablation/);
  });
  test('latency table', () => {
    const h = M._rangemaster_create();
    try { for (const [os, latency] of [[1,0],[2,30.5],[4,37.75],[8,39.875],[16,40.6875]]) {
      expect(applyFullConfigToHandle(M,h,{...cfg(),oversampling:os} as any).ok).toBe(true);
      expect(M._rangemaster_prepare(h,48000,128)).toBe(1); expect(M._rangemaster_get_latency_samples(h)).toBe(latency);
    } } finally { M._rangemaster_destroy(h); }
  });
});
describe('quantum state machine', () => {
  test('identical A/B is sample-exact and preserves state', async () => {
    const p = await processor(), reference = await processor(); let max = 0;
    for (let b=0;b<100;b++) {
      if (b===50) p.handleMessage({type:'set-config',requestId:1,slot:'B',config:cfg(),levelMatchGain:1});
      const a=block(p,b), r=block(reference,b); for(let i=0;i<128;i++) max=Math.max(max,Math.abs(a[i]-r[i]));
    }
    expect(max).toBe(0); expect(p.activeSlot).toBe('B');
  });
  test.each(['sine','impulse','noise','multitone'])('transitions and latest valid request with %s', async kind => {
    const p = await processor();
    for(let b=0;b<150;b++) block(p,b,kind);
    const inputPtr = p.inPtr;
    p.handleMessage({type:'set-config',requestId:1,slot:'B',config:{...cfg(),oversampling:1},levelMatchGain:.8});
    block(p,150,kind); expect(p.state).toBe('WARMUP');
    p.handleMessage({type:'set-config',requestId:2,slot:'A',config:{...cfg(),oversampling:16},levelMatchGain:.7});
    p.handleMessage({type:'set-config',requestId:3,slot:'B',config:{...cfg(),rangeNf:22},levelMatchGain:.6});
    p.handleMessage({type:'set-config',requestId:4,slot:'A',config:{...cfg(),outputRefVolts:-1},levelMatchGain:1});
    for(let b=151;b<500;b++) { const h=p.hActive; block(p,b,kind); if(h!==p.hActive) expect(p.xfadePos).toBeLessThan(128); }
    expect(p.activeSlot).toBe('B'); expect(p.activeRequestId).toBe(3);
    expect(p.activeConfig.rangeNf).toBe(22); expect(p.currentLevelMatchGain).toBeCloseTo(.6);
    expect(p.inPtr).toBe(inputPtr); expect(p.warmPtr).not.toBe(p.inPtr);
    expect(p.port.messages.some((m:any)=>m.type==='param-rejected'&&m.requestId===4)).toBe(true);
  });
  test('dry and bypass have exactly 73 samples delay', async () => {
    const p = await processor(); p.bypassed=true; p.bypassMix=1;
    const out=block(p,0,'impulse'); expect(out[73]).toBe(.5);
    expect(out.filter((v:number)=>v!==0).length).toBe(1);
  });
  test('largest supported quantum catches up without reusing current input', async () => {
    const p = await processor(), input = new Float32Array(1024).fill(.1), out = new Float32Array(1024);
    for (let b=0;b<32;b++) p.process([[input]],[[out]]);
    p.handleMessage({type:'set-config',requestId:1,slot:'B',config:{...cfg(),oversampling:1},levelMatchGain:1});
    for (let b=0;b<40;b++) p.process([[input]],[[out]]);
    expect(p.activeSlot).toBe('B'); expect(p.state).toBe('IDLE');
    expect(Array.from(p.inView.slice(0,1024))).toEqual(Array.from(input));
  });
  test('WASM initialization failure is reported', async () => {
    const p=new Processor(); await p.initWasm(new ArrayBuffer(20),cfg(),'A');
    expect(p.ready).toBe(false); expect(p.port.messages.at(-1).type).toBe('wasm-error');
  });
});
describe('offline listening and export', () => {
  test('identical RMS match, silence handling and no excerpt truncation', () => {
    const input=Float32Array.from({length:48000},(_,i)=>.2*Math.sin(i*.057));
    const gains=computeFileLevelMatchGains(M,input,48000,cfg(),cfg());
    expect(gains.gainA).toBe(gains.gainB);
    expect(()=>computeFileLevelMatchGains(M,new Float32Array(48000),48000,cfg(),cfg())).toThrow(/Silent/);
  });
  test('WAV and manifest include build, ABI, hash and tail semantics', async () => {
    const input=Float32Array.from({length:1024},(_,i)=>.2*Math.sin(i*.057));
    const result=await exportWavAndManifest(M,input,48000,{mode:'full-tail',target:'A',levelMatched:false,matchGain:1,includeMonitoring:false,monitorMode:'direct',cabinetIrBuffer:null,monitorVolume:1,inputLabel:'test',slotConfig:cfg()});
    const manifest=JSON.parse(result.manifestJson); expect(manifest.input_sha256).toHaveLength(64); expect(manifest.config_abi_version).toBe(1);
    expect(result.wavBlob.size).toBe(44+(1024+96000)*4);
    const wav=new DataView(await encodeFloat32Wav(input,48000).arrayBuffer()); expect(wav.getUint16(20,true)).toBe(3); expect(wav.getUint32(40,true)).toBe(4096);
  });
});

test('warmup history length investigation on every engine', () => {
  const history=Float32Array.from({length:48000},(_,i)=>(i<43000?.6:.2)*Math.sin(i*.057));
  const rows=[];
  for(const engine of ['em-dc','em-ac','gp-dc','gp-ac','calibrated-oc44']) {
    const config={...cfg(),engine} as any;
    const full=renderWasmSlice(M,history,48000,config);
    for(const n of [256,512,1024,2048,4096,8192,16384,32768]) {
      const start=history.length-128-n;
      const short=renderWasmSlice(M,history.slice(start),48000,config);
      let sq=0, max=0;
      for(let i=0;i<128;i++){const d=short[n+i]-full[full.length-128+i];sq+=d*d;max=Math.max(max,Math.abs(d));}
      rows.push({engine,historySamples:n,rmsResidual:Math.sqrt(sq/128),maxResidual:max});
    }
  }
  fs.mkdirSync('../artifacts/m71',{recursive:true}); fs.writeFileSync('../artifacts/m71/warmup.json',JSON.stringify(rows,null,2));
  expect(rows.every(r=>Number.isFinite(r.rmsResidual))).toBe(true);
});

test('fractional delay magnitude and phase investigation', () => {
  const rows=[];
  for (const frac of [.125,.25,.3125,.5]) {
    for (const method of ['linear','cubic','sinc63']) {
      const taps=method==='linear' ? [1-frac,frac] : method==='cubic' ? [(1-frac)*(2-frac)*(3-frac)/6,frac*(2-frac)*(3-frac)/2,-frac*(1-frac)*(3-frac)/2,frac*(1-frac)*(2-frac)/6] : Array.from(fractionalCoefficients(frac));
      let maxDb=0,maxPhase=0;
      for(let f=20;f<=20000;f+=20){
        const w=2*Math.PI*f/48000;
        let re=0,im=0;
        taps.forEach((t,j)=>{const phase=-w*(j-(method==='sinc63'?31:0));re+=t*Math.cos(phase);im+=t*Math.sin(phase);});
        const db=20*Math.log10(Math.hypot(re,im));
        const phase=Math.atan2(im,re)+w*frac;
        maxDb=Math.max(maxDb,Math.abs(db));maxPhase=Math.max(maxPhase,Math.abs(phase));
      }
      rows.push({frac,method,maxMagnitudeErrorDb:maxDb,maxPhaseErrorRadians:maxPhase});
      if(method==='sinc63'){expect(maxDb).toBeLessThan(.01);expect(maxPhase).toBeLessThan(.001);}
    }
  }
  fs.writeFileSync('../artifacts/m71/fractional-delay.json',JSON.stringify(rows,null,2));
});
