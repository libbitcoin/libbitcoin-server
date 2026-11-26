// Sample devices shown until the node reports the sessions of its stratum
// services. Values are fixed, times are relative to the moment shown.

// A hashrate at a time, in milliseconds.
export interface HashrateSample {
  time: number;
  rate: number;
}

export interface SampleShare {
  // Seconds before now.
  ago: number;
  template: number;
  difficulty: number;
  // Rejection reason, accepted if none.
  rejected?: string;
}

export interface SampleDevice {
  id: string;
  // The node service the device is connected to.
  service: string;
  worker: string;
  client: string;
  transport: string;
  address: string;
  // Seconds connected before now.
  connected: number;
  // Hashes per second, the average of the history.
  rate: number;
  difficulty: number;
  accepted: number;
  rejected: number;
  // Seconds since the last share.
  lastShare: number;
  shares: SampleShare[];
  seed: number;
}

export const SAMPLE_DEVICES: readonly SampleDevice[] = [
  {
    id: 'ant04',
    service: 'Stratum v1',
    worker: 'ant04',
    client: 'bosminer',
    transport: 'tcp',
    address: '172.21.7.4',
    connected: 58 * 60,
    rate: 13.6e12,
    difficulty: 23_795,
    accepted: 362,
    rejected: 1,
    lastShare: 4,
    shares: [
      { ago: 4, template: 8333, difficulty: 23_795 },
      { ago: 31, template: 8333, difficulty: 23_795 },
      { ago: 58, template: 8333, difficulty: 23_795, rejected: 'Duplicate share' },
      { ago: 71, template: 8332, difficulty: 23_795 },
      { ago: 96, template: 8332, difficulty: 21_880 },
    ],
    seed: 2,
  },
  {
    id: 'ant05',
    service: 'Stratum v1',
    worker: 'ant05',
    client: 'bosminer',
    transport: 'tcp',
    address: '172.21.7.5',
    connected: 58 * 60,
    rate: 13.4e12,
    difficulty: 23_104,
    accepted: 358,
    rejected: 0,
    lastShare: 9,
    shares: [
      { ago: 9, template: 8333, difficulty: 23_104 },
      { ago: 22, template: 8333, difficulty: 23_104 },
      { ago: 40, template: 8333, difficulty: 23_104 },
      { ago: 67, template: 8332, difficulty: 23_104 },
      { ago: 85, template: 8332, difficulty: 21_310 },
    ],
    seed: 3,
  },
  {
    id: 'ant06',
    service: 'Stratum v1',
    worker: 'ant06',
    client: 'bosminer',
    transport: 'tcp',
    address: '172.21.7.6',
    connected: 58 * 60,
    rate: 13.6e12,
    difficulty: 24_010,
    accepted: 364,
    rejected: 0,
    lastShare: 2,
    shares: [
      { ago: 2, template: 8333, difficulty: 24_010 },
      { ago: 19, template: 8333, difficulty: 24_010 },
      { ago: 44, template: 8333, difficulty: 24_010 },
      { ago: 63, template: 8332, difficulty: 24_010 },
      { ago: 90, template: 8332, difficulty: 22_450 },
    ],
    seed: 5,
  },
];

// A minute by minute history of the device over the span, ending now.
export function sampleHistory(device: SampleDevice, now: number, span = 1800): HashrateSample[] {
  const points = Math.floor(span / 60) + 1;
  return Array.from({ length: points }, (_, index) => {
    const wobble =
      Math.sin(index / 3 + device.seed) * 0.05 + Math.cos(index / 1.7 + device.seed * 2) * 0.03;
    return { time: now - (points - 1 - index) * 60_000, rate: device.rate * (1 + wobble) };
  });
}
