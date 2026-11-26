import MinerWorker from './miner.worker?worker&inline';
import type { WorkerRequest, WorkerResponse } from './miner.worker';
import { headerWork } from './sha256';
import {
  bitsTarget,
  buildHeader,
  coinbaseHeight,
  coinbaseTag,
  difficultyTarget,
  extranonce,
  hashValue,
  headerHash,
  parseJob,
  previousHash,
  submitParams,
  suggestedDifficulty,
  targetDifficulty,
  targetWords,
  withNonce,
  type Job,
} from './stratum';

export interface MinerSettings {
  url: string;
  worker: string;
  password: string;
  threads: number;
}

export interface HashrateSample {
  time: number;
  rate: number;
}

export type MinerStatus = 'stopped' | 'connecting' | 'waiting' | 'mining' | 'failed';

export type ShareOutcome = 'pending' | 'accepted' | 'rejected' | 'unanswered';

export interface Share {
  key: number;
  time: number;
  job: string;
  height?: number;
  nonce: string;
  hash: string;
  difficulty: number;
  block: boolean;
  outcome: ShareOutcome;
  reason?: string;
}

export interface MinerJob {
  id: string;
  height?: number;
  tag?: string;
  previous: string;
  networkDifficulty: number;
  time: number;
  clean: boolean;
  received: number;
}

export interface MinerState {
  status: MinerStatus;
  message?: string;
  settings?: MinerSettings;
  started?: number;
  difficulty?: number;
  job?: MinerJob;
  jobs: number;
  hashrate: number;
  history: HashrateSample[];
  hashes: number;
  accepted: number;
  rejected: number;
  blocks: number;
  shares: Share[];
}

// Shares kept for display.
const SHARES = 50;

// Seconds of hashrate kept for display, one sample per second.
const HISTORY = 300;

// Seconds of hashing averaged by the hashrate.
const WINDOW = 10;

// Nonce space of one unit of work (all of it, per extranonce2).
const NONCES = 2 ** 32;

// Seconds a share waits for its answer before the connection is taken as lost.
const ANSWER = 30;

// Seconds before reconnecting a lost connection, doubled per attempt to a cap.
const RETRY = 5;
const RETRY_CAP = 60;

const SUBSCRIBE = 1;
const AUTHORIZE = 2;
const SUGGEST = 3;

const initial: MinerState = {
  status: 'stopped',
  jobs: 0,
  hashrate: 0,
  history: [],
  hashes: 0,
  accepted: 0,
  rejected: 0,
  blocks: 0,
  shares: [],
};

interface Unit {
  job: Job;
  extranonce2: string;
  header: Uint8Array;
}

function reason(error: unknown): string {
  if (Array.isArray(error)) return String(error[1] ?? error[0] ?? 'rejected');
  if (error && typeof error === 'object' && 'message' in error) {
    return String((error as { message: unknown }).message);
  }
  return typeof error === 'string' ? error : 'rejected';
}

class Miner {
  private state: MinerState = initial;
  private listeners = new Set<() => void>();
  private socket?: WebSocket;
  private workers: Worker[] = [];
  private units = new Map<number, Unit>();
  private assigned = new Map<Worker, number>();
  private unitCounter = 0;
  private extranonceCounter = 0;
  private extranonce1 = '';
  private extranonce2Size = 4;
  private job?: Job;
  private target = difficultyTarget(1);
  private requestId = 10;
  private pending = new Map<number, { key: number; sent: number }>();
  // Mining is wanted (started and not stopped), so a lost connection retries.
  private wanted = false;
  // The current run has been subscribed, so a lost connection is not a fault
  // of the settings.
  private subscribed = false;
  private attempts = 0;
  private retry?: ReturnType<typeof setTimeout>;
  private shareKey = 0;
  private samples: { time: number; hashes: number }[] = [];
  private counted = 0;
  private timer?: ReturnType<typeof setInterval>;
  private buffer = '';

  subscribe = (listener: () => void) => {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  };

  getSnapshot = () => this.state;

  private update(change: Partial<MinerState>) {
    this.state = { ...this.state, ...change };
    for (const listener of this.listeners) listener();
  }

  start(settings: MinerSettings) {
    this.stop();
    this.update({ ...initial, status: 'connecting', settings, started: Date.now() });
    this.extranonceCounter = 0;
    this.samples = [];
    this.counted = 0;
    this.wanted = true;
    this.subscribed = false;
    this.attempts = 0;
    this.connect(settings);
  }

  private connect(settings: MinerSettings) {
    this.buffer = '';
    let socket: WebSocket;
    try {
      socket = new WebSocket(settings.url);
    } catch (error) {
      this.fail(error instanceof Error ? error.message : 'Invalid stratum URL.');
      return;
    }

    this.socket = socket;
    socket.onopen = () => {
      this.send(SUBSCRIBE, 'mining.subscribe', ['libbitcoin-web/1.0']);
      this.send(AUTHORIZE, 'mining.authorize', [settings.worker, settings.password]);
    };
    socket.onmessage = (event: MessageEvent) => {
      if (typeof event.data === 'string') this.receive(event.data);
    };
    socket.onerror = () => {
      if (this.socket === socket) this.lose(`Cannot connect to ${settings.url}.`);
    };
    socket.onclose = (event) => {
      if (this.socket !== socket) return;
      this.lose(event.reason ? `Disconnected: ${event.reason}` : 'Disconnected by the server.');
    };

    if (!this.timer) {
      this.timer = setInterval(() => {
        this.sample();
      }, 1000);
    }
  }

  stop() {
    this.wanted = false;
    if (this.retry) clearTimeout(this.retry);
    this.retry = undefined;
    const socket = this.socket;
    this.socket = undefined;
    socket?.close();
    this.unanswered();
    this.halt();
    if (this.state.status !== 'stopped' && this.state.status !== 'failed') {
      this.update({ status: 'stopped', message: undefined, hashrate: 0 });
    }
  }

  // Shares awaiting an answer are given up on.
  private unanswered() {
    if (this.pending.size === 0) return;
    const keys = new Set([...this.pending.values()].map((entry) => entry.key));
    this.pending.clear();
    this.update({
      shares: this.state.shares.map((share) =>
        keys.has(share.key) ? { ...share, outcome: 'unanswered' } : share
      ),
    });
  }

  // A connection that had worked is retried while mining is wanted, a first
  // connection that fails is a fault of the settings.
  private lose(message: string) {
    if (!this.subscribed || !this.wanted) {
      this.fail(message);
      return;
    }

    const socket = this.socket;
    this.socket = undefined;
    socket?.close();
    this.unanswered();
    this.halt(false);
    const seconds = Math.min(RETRY_CAP, RETRY * 2 ** this.attempts);
    this.attempts += 1;
    this.update({
      status: 'connecting',
      message: `${message} Reconnecting in ${String(seconds)} seconds.`,
      hashrate: 0,
    });
    this.retry = setTimeout(() => {
      this.retry = undefined;
      const settings = this.state.settings;
      if (this.wanted && settings) this.connect(settings);
    }, seconds * 1000);
  }

  // Stops hashing, and the clock unless a reconnect keeps sampling.
  private halt(clock = true) {
    if (clock && this.timer) clearInterval(this.timer);
    if (clock) this.timer = undefined;
    for (const worker of this.workers) worker.terminate();
    this.workers = [];
    this.assigned.clear();
    this.units.clear();
    this.pending.clear();
    this.job = undefined;
  }

  private fail(message: string) {
    this.wanted = false;
    if (this.retry) clearTimeout(this.retry);
    this.retry = undefined;
    const socket = this.socket;
    this.socket = undefined;
    socket?.close();
    this.unanswered();
    this.halt();
    this.update({ status: 'failed', message, hashrate: 0 });
  }

  private send(id: number, method: string, params: unknown[]) {
    this.socket?.send(JSON.stringify({ id, method, params }));
  }

  // Messages may be split across or combined within frames, so complete json
  // values are taken from the text received, the rest kept for more. A json
  // array is a batch of messages.
  private receive(data: string) {
    this.buffer += data;
    for (const text of takeJson(this)) {
      let message: unknown;
      try {
        message = JSON.parse(text);
      } catch {
        continue;
      }
      for (const entry of Array.isArray(message) ? message : [message]) {
        if (entry && typeof entry === 'object') this.handle(entry as Record<string, unknown>);
      }
    }
  }

  // The buffer, for takeJson.
  get text(): string {
    return this.buffer;
  }

  set text(value: string) {
    this.buffer = value.length > 1_000_000 ? '' : value;
  }

  private handle(message: Record<string, unknown>) {
    const method = message.method;
    if (typeof method === 'string') {
      const params = message.params;
      if (method === 'mining.set_difficulty' && Array.isArray(params)) {
        const difficulty = Number(params[0]);
        if (difficulty > 0) this.setDifficulty(difficulty);
      } else if (method === 'mining.notify') {
        const job = parseJob(params);
        if (job) this.setJob(job);
      }
      return;
    }

    const id = Number(message.id);
    const error = message.error;
    if (id === SUBSCRIBE) {
      const result = message.result;
      if (error || !Array.isArray(result) || typeof result[1] !== 'string') {
        this.fail(`Subscribe failed: ${reason(error)}`);
        return;
      }
      this.extranonce1 = result[1];
      this.extranonce2Size = Number(result[2]) || 4;
      const threads = this.state.settings?.threads ?? 1;
      this.send(SUGGEST, 'mining.suggest_difficulty', [suggestedDifficulty(threads)]);
      this.subscribed = true;
      this.attempts = 0;
      if (this.state.status === 'connecting') this.update({ status: 'waiting', message: undefined });
      return;
    }

    if (id === AUTHORIZE) {
      if (error || message.result !== true) this.fail(`Worker not authorized: ${reason(error)}`);
      return;
    }

    const key = this.pending.get(id)?.key;
    if (key === undefined) return;
    this.pending.delete(id);
    const accepted = message.result === true && !error;
    this.update({
      accepted: this.state.accepted + (accepted ? 1 : 0),
      rejected: this.state.rejected + (accepted ? 0 : 1),
      shares: this.state.shares.map((share) =>
        share.key === key
          ? {
              ...share,
              outcome: accepted ? 'accepted' : 'rejected',
              reason: accepted ? undefined : reason(error),
            }
          : share
      ),
    });
  }

  private setDifficulty(difficulty: number) {
    this.target = difficultyTarget(difficulty);
    this.update({ difficulty });
    const target = targetWords(this.target);
    for (const worker of this.workers) this.post(worker, { type: 'target', target });
  }

  private setJob(job: Job) {
    this.job = job;
    this.update({
      status: 'mining',
      jobs: this.state.jobs + 1,
      job: {
        id: job.id,
        height: coinbaseHeight(job),
        tag: coinbaseTag(job),
        previous: previousHash(job),
        networkDifficulty: targetDifficulty(bitsTarget(job.bits)),
        time: Number.parseInt(job.time, 16),
        clean: job.clean,
        received: Date.now(),
      },
    });

    const threads = this.state.settings?.threads ?? 1;
    while (this.workers.length < threads) this.spawn();

    // Every worker moves to the latest job.
    for (const worker of this.workers) this.assign(worker);
  }

  private spawn() {
    const worker = new MinerWorker();
    worker.onmessage = (event: MessageEvent<WorkerResponse>) => {
      this.fromWorker(worker, event.data);
    };
    this.workers.push(worker);
  }

  private post(worker: Worker, request: WorkerRequest) {
    worker.postMessage(request);
  }

  private assign(worker: Worker) {
    const job = this.job;
    if (!job) return;
    const previous = this.assigned.get(worker);
    if (previous !== undefined) this.units.delete(previous);

    const extranonce2 = extranonce(this.extranonceCounter++, this.extranonce2Size);
    const header = buildHeader(job, this.extranonce1, extranonce2);
    const unit = ++this.unitCounter;
    this.units.set(unit, { job, extranonce2, header });
    this.assigned.set(worker, unit);
    this.post(worker, {
      type: 'work',
      unit,
      work: headerWork(header),
      target: targetWords(this.target),
      start: 0,
      end: NONCES,
    });
  }

  private fromWorker(worker: Worker, response: WorkerResponse) {
    if (response.type === 'progress') {
      this.counted += response.hashes;
      return;
    }

    const unit = this.units.get(response.unit);
    if (!unit) return;
    if (response.type === 'done') {
      this.assign(worker);
      return;
    }

    this.found(unit, response.nonce);
  }

  private found(unit: Unit, nonce: number) {
    const header = withNonce(unit.header, nonce);
    const value = hashValue(header);

    // Checked here too, as the target may have changed since.
    if (value > this.target) return;
    const block = value <= bitsTarget(unit.job.bits);
    const id = ++this.requestId;
    const key = ++this.shareKey;
    this.pending.set(id, { key, sent: Date.now() });
    this.send(id, 'mining.submit', submitParams(this.state.settings?.worker ?? '', unit.job, unit.extranonce2, nonce));
    const share: Share = {
      key,
      time: Date.now(),
      job: unit.job.id,
      height: coinbaseHeight(unit.job),
      nonce: (nonce >>> 0).toString(16).padStart(8, '0'),
      hash: headerHash(header),
      difficulty: targetDifficulty(value),
      block,
      outcome: 'pending',
    };
    this.update({
      blocks: this.state.blocks + (block ? 1 : 0),
      shares: [share, ...this.state.shares].slice(0, SHARES),
    });
  }

  private sample() {
    const time = Date.now();
    for (const entry of this.pending.values()) {
      if (time - entry.sent > ANSWER * 1000) {
        this.lose(`No answer from the stratum service for ${String(ANSWER)} seconds.`);
        return;
      }
    }

    this.samples.push({ time, hashes: this.counted });
    while (this.samples.length > WINDOW + 1) this.samples.shift();
    const first = this.samples[0];
    const seconds = (time - first.time) / 1000;
    const hashrate = seconds > 0 ? (this.counted - first.hashes) / seconds : 0;
    const history = [...this.state.history, { time, rate: hashrate }].slice(-HISTORY);
    this.update({ hashrate, history, hashes: this.counted });
  }
}

export const miner = new Miner();

// Takes the complete json values (objects or arrays) from the text of the
// holder, leaving any incomplete value as its text.
export function takeJson(holder: { text: string }): string[] {
  const text = holder.text;
  const values: string[] = [];
  let depth = 0;
  let start = -1;
  let quoted = false;
  let escaped = false;
  let consumed = 0;
  for (let index = 0; index < text.length; index++) {
    const character = text[index];
    if (quoted) {
      if (escaped) escaped = false;
      else if (character === '\\') escaped = true;
      else if (character === '"') quoted = false;
      continue;
    }

    if (character === '"') {
      quoted = depth > 0;
    } else if (character === '{' || character === '[') {
      if (depth === 0) start = index;
      depth++;
    } else if (character === '}' || character === ']') {
      if (depth === 0) {
        consumed = index + 1;
        continue;
      }
      depth--;
      if (depth === 0) {
        values.push(text.slice(start, index + 1));
        consumed = index + 1;
      }
    } else if (depth === 0) {
      consumed = index + 1;
    }
  }

  holder.text = text.slice(consumed);
  return values;
}
