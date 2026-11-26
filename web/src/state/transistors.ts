export interface TransistorMetadata {
  index: number;
  id: string;
  label: string;
  partNumber: string;
  family: 'OC44' | 'AC128';
  citation: string;
  detail: string;
  extractionMethod: string;
  hasCeb: boolean;
  hasCcb: boolean;
  cebPf: number;
  ccbPf: number;
  nominalVcVolts: number; // GP-AC nominal DC Vc at 9V, biasTrim=1.0
}

export const TRANSISTOR_CATALOG: TransistorMetadata[] = [
  {
    index: 0,
    id: 'oc44-01',
    label: 'OC44 #1',
    partNumber: 'Mullard OC44 (#1)',
    family: 'OC44',
    citation: 'Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3',
    detail: 'Thesis pp. 71, 80 (PDF pp. 95, 104); Is=1.423 uA, betaF=307, Vaf=8.17 V',
    extractionMethod: 'Keithley 2602B SMU pulsed I-V + Nelder-Mead/interior-point fit; OC44 datasheet Ceb/Ccb',
    hasCeb: true,
    hasCcb: true,
    cebPf: 410.0,
    ccbPf: 10.5,
    nominalVcVolts: -6.596,
  },
  {
    index: 1,
    id: 'oc44-02',
    label: 'OC44 #2',
    partNumber: 'Mullard OC44 (#2*)',
    family: 'OC44',
    citation: 'Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3',
    detail: 'Best-fit VA reference specimen; Is=2.245 uA, betaF=283.5, Vaf=6.86 V',
    extractionMethod: 'Keithley 2602B SMU pulsed I-V + optimization fit; OC44 datasheet Ceb/Ccb',
    hasCeb: true,
    hasCcb: true,
    cebPf: 410.0,
    ccbPf: 10.5,
    nominalVcVolts: -6.611,
  },
  {
    index: 2,
    id: 'oc44-03',
    label: 'OC44 #3',
    partNumber: 'Mullard OC44 (#3)',
    family: 'OC44',
    citation: 'Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3',
    detail: 'Higher knee current specimen; Is=2.867 uA, betaF=226.2, Vaf=8.79 V',
    extractionMethod: 'Keithley 2602B SMU pulsed I-V + optimization fit; OC44 datasheet Ceb/Ccb',
    hasCeb: true,
    hasCcb: true,
    cebPf: 410.0,
    ccbPf: 10.5,
    nominalVcVolts: -6.556,
  },
  {
    index: 3,
    id: 'oc44-dafx17',
    label: 'OC44 DAFx17',
    partNumber: 'Mullard OC44 (DAFx-17 Table 1)',
    family: 'OC44',
    citation: 'Holmes, Holters & van Walstijn (2017) DAFx-17 Table 1',
    detail: 'Published DAFx-17 Fig. 10 reference specimen; Ccb=10 pF',
    extractionMethod: 'Keithley 2602B SMU pulsed I-V + multi-stage optimization; OC44 datasheet',
    hasCeb: true,
    hasCcb: true,
    cebPf: 410.0,
    ccbPf: 10.0,
    nominalVcVolts: -6.596,
  },
  {
    index: 4,
    id: 'ac128-01',
    label: 'AC128 #1',
    partNumber: 'AC128 (#1*)',
    family: 'AC128',
    citation: 'Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3',
    detail: 'Best-fit AC128 specimen; higher Is (27.8 uA) & leakage; Ceb absent in datasheet',
    extractionMethod: 'Keithley 2602B SMU pulsed I-V; Ccb=100 pF from datasheet (Ceb missing)',
    hasCeb: false,
    hasCcb: true,
    cebPf: 0.0,
    ccbPf: 100.0,
    nominalVcVolts: -6.336,
  },
  {
    index: 5,
    id: 'ac128-02',
    label: 'AC128 #2',
    partNumber: 'AC128 (#2)',
    family: 'AC128',
    citation: 'Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3',
    detail: 'Is=31.67 uA, betaF=125.7, Vaf=61.24 V; Ceb absent in datasheet',
    extractionMethod: 'Keithley 2602B SMU pulsed I-V; Ccb=100 pF from datasheet (Ceb missing)',
    hasCeb: false,
    hasCcb: true,
    cebPf: 0.0,
    ccbPf: 100.0,
    nominalVcVolts: -6.556,
  },
  {
    index: 6,
    id: 'ac128-03',
    label: 'AC128 #3',
    partNumber: 'AC128 (#3)',
    family: 'AC128',
    citation: 'Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3',
    detail: 'High reverse CB leakage (negative quiescent Ib); Ceb absent in datasheet',
    extractionMethod: 'Keithley 2602B SMU pulsed I-V; Ccb=100 pF from datasheet (Ceb missing)',
    hasCeb: false,
    hasCcb: true,
    cebPf: 0.0,
    ccbPf: 100.0,
    nominalVcVolts: -6.116,
  },
  {
    index: 7,
    id: 'ac128-04',
    label: 'AC128 #4',
    partNumber: 'AC128 (#4)',
    family: 'AC128',
    citation: 'Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3 (DAFx-17 Table 1)',
    detail: 'Is=20.66 uA, betaF=229.6; Ceb absent in datasheet',
    extractionMethod: 'Keithley 2602B SMU pulsed I-V; Ccb=100 pF from datasheet (Ceb missing)',
    hasCeb: false,
    hasCcb: true,
    cebPf: 0.0,
    ccbPf: 100.0,
    nominalVcVolts: -6.166,
  },
];

export function getTransistorMeta(id: string): TransistorMetadata {
  return TRANSISTOR_CATALOG.find((t) => t.id === id) ?? TRANSISTOR_CATALOG[1];
}
