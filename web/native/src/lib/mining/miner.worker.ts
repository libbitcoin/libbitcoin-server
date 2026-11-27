import { simdScanner } from './scan-simd';
import { scan, type HeaderWork, type Target } from './sha256';

export type WorkerRequest =
  | { type: 'work'; unit: number; work: HeaderWork; target: Target; start: number; end: number }
  | { type: 'target'; target: Target }
  | { type: 'stop' };

export type WorkerResponse =
  | { type: 'found'; unit: number; nonce: number }
  | { type: 'progress'; unit: number; hashes: number; simd: boolean }
  | { type: 'done'; unit: number };

const scope = globalThis as unknown as {
  postMessage(message: WorkerResponse): void;
  onmessage: ((event: MessageEvent<WorkerRequest>) => void) | null;
};

// Nonces hashed between yields to the message loop.
const CHUNK = 1 << 16;

// WebAssembly SIMD where supported, else the JavaScript scanner.
const simd = simdScanner();
const hash = simd ?? scan;

let current: { unit: number; work: HeaderWork; next: number; end: number } | undefined;
let target: Target | undefined;
let scheduled = false;

function step() {
  scheduled = false;
  const unit = current;
  if (!unit || !target) return;
  const end = Math.min(unit.next + CHUNK, unit.end);
  hash(unit.work, target, unit.next, end, (nonce) => {
    scope.postMessage({ type: 'found', unit: unit.unit, nonce });
  });
  scope.postMessage({ type: 'progress', unit: unit.unit, hashes: end - unit.next, simd: simd !== undefined });
  unit.next = end;
  if (end >= unit.end) {
    current = undefined;
    scope.postMessage({ type: 'done', unit: unit.unit });
    return;
  }
  schedule();
}

function schedule() {
  if (scheduled) return;
  scheduled = true;
  setTimeout(step, 0);
}

scope.onmessage = (event) => {
  const request = event.data;
  if (request.type === 'stop') {
    current = undefined;
    return;
  }
  target = request.target;
  if (request.type === 'work') {
    current = { unit: request.unit, work: request.work, next: request.start, end: request.end };
    schedule();
  }
};
