import { DspSlotConfig } from './types';

export interface PresetDefinition {
  id: string;
  name: string;
  description: string;
  config: DspSlotConfig;
}

export const NOMINAL_DEFAULT_CONFIG: DspSlotConfig = {
  engine: 'gp-ac',
  transistorId: 'oc44-02',
  oversampling: 4,
  boost: 1.0,
  potTaper: 0,
  boostLinearApprox: false,
  rangeNf: 4.7,
  inputVolts: 0.35,
  outputRefVolts: 9.0,
  outputTrimDb: 0.0,
  biasTrim: 1.0,
  supplyVolts: 9.0,
  sourceOhms: 1000.0,
  loadOhms: 1000000.0,
  gpAblation: {
    enableJunctionCaps: true,
    enableEarlyEffect: true,
    enableLeakageCurrents: true,
    enableHighCurrentRolloff: true,
    enableTerminalResistances: true,
  },
};

export const PRESET_LIST: PresetDefinition[] = [
  {
    id: 'original-oc44',
    name: 'Original OC44',
    description: 'Reference GP-AC + OC44 #2, 4.7 nF Original Range, 100% Boost, 0.35 V/FS.',
    config: { ...NOMINAL_DEFAULT_CONFIG },
  },
  {
    id: 'vintage-narrow',
    name: 'Vintage Narrow',
    description: 'OC44 #1 with 2.2 nF Narrow input capacitor for tight upper-mid focus.',
    config: { ...NOMINAL_DEFAULT_CONFIG, transistorId: 'oc44-01', rangeNf: 2.2 },
  },
  {
    id: 'midrange-push',
    name: 'Midrange Push',
    description: '10 nF Mid cap, 90% Boost, 0.45 V/FS for thicker vocal midrange.',
    config: { ...NOMINAL_DEFAULT_CONFIG, rangeNf: 10.0, boost: 0.9, inputVolts: 0.45 },
  },
  {
    id: 'wide-range',
    name: 'Wide Range',
    description: '22 nF Wide cap, 85% Boost, 0.40 V/FS for fuller low-mid response.',
    config: { ...NOMINAL_DEFAULT_CONFIG, rangeNf: 22.0, boost: 0.85, inputVolts: 0.4 },
  },
  {
    id: 'full-range',
    name: 'Full Range',
    description: 'OC44 #3 with 47 nF cap, 85% Boost, 0.35 V/FS for extended bass.',
    config: { ...NOMINAL_DEFAULT_CONFIG, transistorId: 'oc44-03', rangeNf: 47.0, boost: 0.85 },
  },
  {
    id: 'low-input-clean',
    name: 'Low Input / Clean Boost',
    description: '0.12 V/FS and 70% Boost for low-THD dynamic treble lift.',
    config: { ...NOMINAL_DEFAULT_CONFIG, inputVolts: 0.12, boost: 0.7 },
  },
  {
    id: 'high-input-drive',
    name: 'High Input / Germanium Drive',
    description: '0.90 V/FS driving the PNP stage deeply into asymmetric germanium clipping.',
    config: { ...NOMINAL_DEFAULT_CONFIG, inputVolts: 0.9, boost: 1.0, outputTrimDb: -6.0 },
  },
  {
    id: 'ac128-alternative',
    name: 'AC128 Alternative',
    description: 'AC128 #1* profile (higher leakage, 100 pF Ccb, missing Ceb in datasheet).',
    config: { ...NOMINAL_DEFAULT_CONFIG, transistorId: 'ac128-01', inputVolts: 0.4 },
  },
  {
    id: 'holmes-calibrated-exp',
    name: 'Holmes Calibrated — Experimental',
    description: 'Engine E (Holmes 2019 Table 6.2) isolated literature parameter set.',
    config: { ...NOMINAL_DEFAULT_CONFIG, engine: 'calibrated-oc44' },
  },
];
