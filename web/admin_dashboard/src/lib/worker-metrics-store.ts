/* eslint-disable unicorn/prefer-add-event-listener -- Replacing the handlers isolates a retiring worker from its replacement. */
import TelemetryWorker from './telemetry.worker?worker&inline';
import { DEFAULT_LOG_FILTER } from './admin-contract';
import { LiveMetricsStore, type MetricsSnapshot } from './live-metrics';
import type { ConnectionStatus } from './reconnecting-websocket';
import type { LogQuery, LogWindow } from './log-window';
import { STREAM_REFRESH_MS } from './stream-window';
import type { EventQuery, EventWindow } from './event-window';
import type { NativeFees } from './native-contract';
import { INITIAL_LIVE_STATE, type NativeLiveState } from './native-client';
import {
  applyMetricsUpdate,
  reuseRows,
  type TelemetryCommand,
  type TelemetryResponse,
} from './telemetry-worker-protocol';

const INITIAL_STATUS: ConnectionStatus = {
  phase: 'idle',
  attempt: 0,
  reconnects: 0,
  connectedAt: null,
  lastMessageAt: null,
  error: null,
};

export class WorkerMetricsStore {
  #worker: Worker | null = null;
  #snapshot: MetricsSnapshot = new LiveMetricsStore().getSnapshot();
  #status: ConnectionStatus = INITIAL_STATUS;
  #listeners = new Set<() => void>();
  #statusListeners = new Set<() => void>();
  #nativeStatus: ConnectionStatus = INITIAL_STATUS;
  #nativeLive: NativeLiveState = INITIAL_LIVE_STATE;
  #nativeListeners = new Set<() => void>();
  #endpoint: string | null = null;
  #nativeEndpoint: string | null = null;
  #logFilter = DEFAULT_LOG_FILTER;
  #generation = 0;
  #requestId = 0;
  #pendingRequest: number | null = null;
  #pollTimer: ReturnType<typeof setInterval> | null = null;
  #logTimer: ReturnType<typeof setInterval> | null = null;
  #logListeners = new Set<() => void>();
  #logWindow: LogWindow = { logs: [], retained: 0, matched: 0, offset: 0 };
  #logQuery: LogQuery = { search: '', filter: DEFAULT_LOG_FILTER, offset: 0 };
  #logQueryVersion = 0;
  #pendingLogRequest: number | null = null;
  #eventTimer: ReturnType<typeof setInterval> | null = null;
  #eventListeners = new Set<() => void>();
  #eventWindow: EventWindow = { events: [], retained: 0, matched: 0, offset: 0 };
  #eventQuery: EventQuery = { search: '', category: 'all', offset: 0 };
  #eventQueryVersion = 0;
  #pendingEventRequest: number | null = null;
  #logsPaused = false;
  #eventsPaused = false;

  constructor(readonly createWorker: () => Worker = () => new TelemetryWorker()) {}

  readonly getSnapshot = (): MetricsSnapshot => this.#snapshot;
  readonly getStatus = (): ConnectionStatus => this.#status;
  readonly getNativeStatus = (): ConnectionStatus => this.#nativeStatus;
  readonly getNativeLive = (): NativeLiveState => this.#nativeLive;
  readonly subscribeNative = (listener: () => void): (() => void) => {
    this.#nativeListeners.add(listener);
    return () => {
      this.#nativeListeners.delete(listener);
    };
  };
  readonly getLogWindow = (): LogWindow => this.#logWindow;
  readonly getEventWindow = (): EventWindow => this.#eventWindow;
  readonly subscribeEvents = (listener: () => void): (() => void) => {
    this.#eventListeners.add(listener);
    this.#requestEvents();
    this.#startEventPolling();
    return () => {
      this.#eventListeners.delete(listener);
      if (this.#eventListeners.size === 0) this.#clearEventPolling();
    };
  };
  readonly subscribeLogs = (listener: () => void): (() => void) => {
    this.#logListeners.add(listener);
    this.#requestLogs();
    this.#startLogPolling();
    return () => {
      this.#logListeners.delete(listener);
      if (this.#logListeners.size === 0) this.#clearLogPolling();
    };
  };
  readonly subscribe = (listener: () => void): (() => void) => {
    this.#listeners.add(listener);
    return () => {
      this.#listeners.delete(listener);
    };
  };
  readonly subscribeStatus = (listener: () => void): (() => void) => {
    this.#statusListeners.add(listener);
    return () => {
      this.#statusListeners.delete(listener);
    };
  };

  configure(endpoint: string, nativeEndpoint: string, logFilter: number): void {
    if (
      endpoint === this.#endpoint &&
      nativeEndpoint === this.#nativeEndpoint &&
      logFilter === this.#logFilter
    )
      return;
    if (this.#endpoint !== null && endpoint !== this.#endpoint) {
      this.reset();
      this.#setStatus({ ...INITIAL_STATUS, phase: 'connecting' });
    }
    if (this.#nativeEndpoint !== null && nativeEndpoint !== this.#nativeEndpoint) {
      this.#setNative({ ...INITIAL_STATUS, phase: 'connecting' }, INITIAL_LIVE_STATE);
    }
    this.#endpoint = endpoint;
    this.#nativeEndpoint = nativeEndpoint;
    this.#logFilter = logFilter;
    this.#sendConfiguration();
  }

  start(): void {
    if (this.#worker || this.#endpoint === null || this.#nativeEndpoint === null) return;
    try {
      const worker = this.createWorker();
      this.#worker = worker;
      this.#logWindow = { logs: [], retained: 0, matched: 0, offset: 0 };
      this.#eventWindow = { events: [], retained: 0, matched: 0, offset: 0 };
      for (const listener of this.#logListeners) listener();
      for (const listener of this.#eventListeners) listener();
      this.#setStatus({ ...INITIAL_STATUS, phase: 'connecting' });
      this.#setNative({ ...INITIAL_STATUS, phase: 'connecting' }, INITIAL_LIVE_STATE);
      worker.onmessage = (event: MessageEvent<TelemetryResponse>) => {
        if (this.#worker !== worker) return;
        const response = event.data;
        if (response.type === 'events') {
          if (response.requestId !== this.#pendingEventRequest) return;
          this.#pendingEventRequest = null;
          if (response.generation !== this.#generation) return;
          if (response.queryVersion !== this.#eventQueryVersion) {
            this.#requestEvents();
            return;
          }
          this.#eventWindow = {
            ...response.window,
            events: reuseRows(this.#eventWindow.events, response.window.events),
          };
          for (const listener of this.#eventListeners) listener();
          return;
        }
        if (response.type === 'logs') {
          if (response.requestId !== this.#pendingLogRequest) return;
          this.#pendingLogRequest = null;
          if (response.generation !== this.#generation) return;
          if (response.queryVersion !== this.#logQueryVersion) {
            this.#requestLogs();
            return;
          }
          this.#logWindow = {
            ...response.window,
            logs: reuseRows(this.#logWindow.logs, response.window.logs),
          };
          for (const listener of this.#logListeners) listener();
          return;
        }
        if (response.type === 'native') {
          this.#setNative(response.status, response.live);
          return;
        }
        if (response.type !== 'snapshot') return;
        if (response.requestId !== this.#pendingRequest) return;
        this.#pendingRequest = null;
        if (response.generation !== this.#generation) return;
        this.#snapshot = applyMetricsUpdate(this.#snapshot, response.update);
        this.#setStatus(response.status);
        for (const listener of this.#listeners) listener();
      };
      worker.onerror = () => {
        this.#fail('Telemetry worker failed. Reload the dashboard to reconnect.');
      };
      worker.onmessageerror = () => {
        this.#fail('Unable to read telemetry worker data. Reload the dashboard to reconnect.');
      };
      this.#sendConfiguration();
      if (this.#logsPaused)
        this.#send({
          type: 'pause-display',
          generation: this.#generation,
          stream: 'logs',
          paused: true,
        });
      if (this.#eventsPaused)
        this.#send({
          type: 'pause-display',
          generation: this.#generation,
          stream: 'events',
          paused: true,
        });
      this.#requestSnapshot();
      if (this.getStatus().error === null)
        this.#pollTimer = setInterval(() => {
          this.#requestSnapshot();
        }, 1_000);
      this.#startLogPolling();
      this.#startEventPolling();
    } catch (error) {
      this.#fail(
        `Unable to start telemetry worker: ${error instanceof Error ? error.message : String(error)}`
      );
    }
  }

  stop(): void {
    this.#clearPolling();
    const worker = this.#worker;
    this.#worker = null;
    if (worker) {
      // Give the socket a chance to unsubscribe and close; don't keep a stuck worker alive.
      const timer = setTimeout(() => {
        worker.terminate();
      }, 500);
      const finish = () => {
        clearTimeout(timer);
        worker.terminate();
      };
      worker.onmessage = (event: MessageEvent<TelemetryResponse>) => {
        if (event.data.type === 'stopped') finish();
      };
      worker.onerror = finish;
      worker.onmessageerror = finish;
      try {
        worker.postMessage({ type: 'stop' } satisfies TelemetryCommand);
      } catch {
        finish();
      }
    }
    this.#setStatus(INITIAL_STATUS);
    this.#setNative(INITIAL_STATUS, INITIAL_LIVE_STATE);
  }

  recordNativeTop(height: number, receivedAt = Date.now()): void {
    this.#send({ type: 'native-top', generation: this.#generation, height, receivedAt });
  }

  recordNativeFees(fees: NativeFees, receivedAt = Date.now()): void {
    this.#send({ type: 'native-fees', generation: this.#generation, fees, receivedAt });
  }

  clearLogs(): void {
    this.#send({ type: 'clear-logs', generation: this.#generation });
    this.#logQueryVersion += 1;
    this.#requestLogs();
  }
  clearEvents(): void {
    this.#send({ type: 'clear-events', generation: this.#generation });
    this.#eventQueryVersion += 1;
    this.#requestEvents();
  }

  setLogsPaused(paused: boolean): void {
    if (paused === this.#logsPaused) return;
    this.#logsPaused = paused;
    this.#clearLogPolling();
    this.#send({ type: 'pause-display', generation: this.#generation, stream: 'logs', paused });
    this.#logQueryVersion += 1;
    this.#requestLogs();
    this.#startLogPolling();
  }

  setEventsPaused(paused: boolean): void {
    if (paused === this.#eventsPaused) return;
    this.#eventsPaused = paused;
    this.#clearEventPolling();
    this.#send({ type: 'pause-display', generation: this.#generation, stream: 'events', paused });
    this.#eventQueryVersion += 1;
    this.#requestEvents();
    this.#startEventPolling();
  }

  reset(): void {
    this.#generation += 1;
    this.#pendingRequest = null;
    this.#pendingLogRequest = null;
    this.#pendingEventRequest = null;
    this.#logWindow = { logs: [], retained: 0, matched: 0, offset: 0 };
    this.#eventWindow = { events: [], retained: 0, matched: 0, offset: 0 };
    this.#snapshot = new LiveMetricsStore().getSnapshot();
    this.#send({ type: 'reset', generation: this.#generation });
    for (const listener of this.#listeners) listener();
    for (const listener of this.#logListeners) listener();
    for (const listener of this.#eventListeners) listener();
  }

  setLogQuery(query: LogQuery): void {
    if (JSON.stringify(query) === JSON.stringify(this.#logQuery)) return;
    this.#logQuery = query;
    this.#logQueryVersion += 1;
    this.#requestLogs();
  }

  setEventQuery(query: EventQuery): void {
    if (JSON.stringify(query) === JSON.stringify(this.#eventQuery)) return;
    this.#eventQuery = query;
    this.#eventQueryVersion += 1;
    this.#requestEvents();
  }

  #startEventPolling(): void {
    if (!this.#worker || this.#eventListeners.size === 0 || this.#eventTimer || this.#eventsPaused)
      return;
    // The worker remains readable through socket errors. Install the timer first
    // so a failed worker request can clear it along with the other polling.
    this.#eventTimer = setInterval(() => {
      this.#requestEvents();
    }, STREAM_REFRESH_MS);
    this.#requestEvents();
  }

  #requestEvents(): void {
    if (this.#pendingEventRequest !== null || !this.#worker || this.#eventListeners.size === 0)
      return;
    this.#pendingEventRequest = ++this.#requestId;
    this.#send({
      type: 'poll-events',
      generation: this.#generation,
      requestId: this.#pendingEventRequest,
      queryVersion: this.#eventQueryVersion,
      query: this.#eventQuery,
    });
  }

  #clearEventPolling(): void {
    if (this.#eventTimer) clearInterval(this.#eventTimer);
    this.#eventTimer = null;
    this.#pendingEventRequest = null;
  }

  #startLogPolling(): void {
    if (!this.#worker || this.#logListeners.size === 0 || this.#logTimer || this.#logsPaused)
      return;
    // Socket errors must not stop display polling; a worker failure clears it.
    this.#logTimer = setInterval(() => {
      this.#requestLogs();
    }, STREAM_REFRESH_MS);
    this.#requestLogs();
  }

  #requestLogs(): void {
    if (this.#pendingLogRequest !== null || !this.#worker || this.#logListeners.size === 0) return;
    this.#pendingLogRequest = ++this.#requestId;
    this.#send({
      type: 'poll-logs',
      generation: this.#generation,
      requestId: this.#pendingLogRequest,
      queryVersion: this.#logQueryVersion,
      query: this.#logQuery,
    });
  }

  #clearLogPolling(): void {
    if (this.#logTimer) clearInterval(this.#logTimer);
    this.#logTimer = null;
    this.#pendingLogRequest = null;
  }

  #sendConfiguration(): void {
    if (this.#endpoint !== null && this.#nativeEndpoint !== null)
      this.#send({
        type: 'configure',
        generation: this.#generation,
        endpoint: this.#endpoint,
        nativeEndpoint: this.#nativeEndpoint,
        logFilter: this.#logFilter,
      });
  }

  #requestSnapshot(): void {
    if (this.#pendingRequest !== null || !this.#worker) return;
    this.#pendingRequest = ++this.#requestId;
    this.#send({ type: 'poll', generation: this.#generation, requestId: this.#pendingRequest });
  }

  #send(command: TelemetryCommand): void {
    try {
      this.#worker?.postMessage(command);
    } catch {
      this.#fail('Unable to communicate with telemetry worker. Reload the dashboard to reconnect.');
    }
  }

  #clearPolling(): void {
    this.#clearLogPolling();
    this.#clearEventPolling();
    if (this.#pollTimer) clearInterval(this.#pollTimer);
    this.#pollTimer = null;
    this.#pendingRequest = null;
  }

  #fail(error: string): void {
    this.#clearPolling();
    this.#worker?.terminate();
    this.#worker = null;
    this.#setStatus({ ...INITIAL_STATUS, error });
    this.#setNative({ ...INITIAL_STATUS, error }, INITIAL_LIVE_STATE);
  }

  #setNative(status: ConnectionStatus, live: NativeLiveState): void {
    this.#nativeStatus = status;
    this.#nativeLive = live;
    for (const listener of this.#nativeListeners) listener();
  }

  #setStatus(status: ConnectionStatus): void {
    if (JSON.stringify(status) === JSON.stringify(this.#status)) return;
    this.#status = status;
    for (const listener of this.#statusListeners) listener();
  }
}
