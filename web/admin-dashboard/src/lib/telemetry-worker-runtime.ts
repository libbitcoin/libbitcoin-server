import { AdminClient } from './admin-client';
import { ALL_EVENT_FILTER } from './admin-contract';
import {
  LiveMetricsStore,
  type MetricsSnapshot,
  type RecentLog,
  type RecentEvent,
} from './live-metrics';
import { selectLogWindow } from './log-window';
import { selectEventWindow } from './event-window';
import {
  createMetricsUpdate,
  type TelemetryCommand,
  type TelemetryResponse,
} from './telemetry-worker-protocol';

type ClientFactory = (endpoint: string, logFilter: number) => AdminClient;

// No DOM or React dependencies: the worker owns receipt, validation and aggregation.
export class TelemetryWorkerRuntime {
  #metrics: LiveMetricsStore;
  #client: AdminClient | null = null;
  #endpoint: string | null = null;
  #generation = 0;
  #previous: MetricsSnapshot | null = null;
  #frozenLogs: readonly RecentLog[] | null = null;
  #frozenEvents: readonly RecentEvent[] | null = null;
  #unsubscribers: Array<() => void> = [];

  constructor(
    readonly send: (message: TelemetryResponse) => void,
    readonly createClient: ClientFactory = (endpoint, logFilter) =>
      new AdminClient(endpoint, { logFilter }),
    now: () => number = Date.now
  ) {
    // Materialize snapshots only when the UI requests one. Collection and pruning
    // continue independently even if the main thread is blocked or backgrounded.
    this.#metrics = new LiveMetricsStore(now, false);
  }

  handle(command: TelemetryCommand): void {
    if (command.type === 'stop') {
      this.#disconnect();
      this.#metrics.stop();
      this.send({ type: 'stopped' });
      return;
    }
    if (command.type === 'configure') {
      if (command.generation < this.#generation) return;
      if (command.generation !== this.#generation || command.endpoint !== this.#endpoint) {
        this.#reset(command.generation);
      }
      if (command.endpoint !== this.#endpoint || this.#client === null) {
        this.#disconnect();
        this.#endpoint = command.endpoint;
        this.#client = this.createClient(command.endpoint, command.logFilter);
        this.#unsubscribers = [
          this.#client.subscribeFrames((frame) => {
            this.#metrics.ingest(frame);
          }),
          this.#client.subscribeInvalidFrames(() => {
            this.#metrics.recordInvalidFrame();
          }),
        ];
        this.#client.start();
        this.#metrics.start();
      } else {
        this.#client.setFilters(command.logFilter, ALL_EVENT_FILTER);
      }
      return;
    }
    if (command.type === 'reset') {
      if (command.generation >= this.#generation) this.#reset(command.generation);
      return;
    }
    if (command.generation !== this.#generation) return;
    switch (command.type) {
      case 'pause-display': {
        if (command.stream === 'logs') {
          this.#frozenLogs = command.paused
            ? (this.#frozenLogs ?? this.#metrics.getRecentLogs())
            : null;
        } else {
          this.#frozenEvents = command.paused
            ? (this.#frozenEvents ?? this.#metrics.getRecentEvents())
            : null;
        }
        break;
      }
      case 'poll-events': {
        this.send({
          type: 'events',
          generation: this.#generation,
          requestId: command.requestId,
          queryVersion: command.queryVersion,
          window: selectEventWindow(
            this.#frozenEvents ?? this.#metrics.getRecentEvents(),
            command.query
          ),
        });
        break;
      }
      case 'poll-logs': {
        this.send({
          type: 'logs',
          generation: this.#generation,
          requestId: command.requestId,
          queryVersion: command.queryVersion,
          window: selectLogWindow(this.#frozenLogs ?? this.#metrics.getRecentLogs(), command.query),
        });
        break;
      }
      case 'native-top': {
        this.#metrics.recordNativeTop(command.height, command.receivedAt);
        break;
      }
      case 'native-fees': {
        this.#metrics.recordNativeFees(command.fees, command.receivedAt);
        break;
      }
      case 'clear-logs': {
        this.#metrics.clearLogs();
        if (this.#frozenLogs !== null) this.#frozenLogs = [];
        break;
      }
      case 'clear-events': {
        this.#metrics.clearEvents();
        if (this.#frozenEvents !== null) this.#frozenEvents = [];
        break;
      }
      case 'poll': {
        if (!this.#client) return;
        this.#metrics.flush();
        const snapshot = this.#metrics.getSnapshot();
        this.send({
          type: 'snapshot',
          generation: this.#generation,
          requestId: command.requestId,
          status: this.#client.getStatus(),
          update: createMetricsUpdate(snapshot, this.#previous),
        });
        this.#previous = snapshot;
        break;
      }
    }
  }

  #reset(generation: number): void {
    this.#generation = generation;
    this.#previous = null;
    if (this.#frozenLogs !== null) this.#frozenLogs = [];
    if (this.#frozenEvents !== null) this.#frozenEvents = [];
    this.#metrics.reset();
  }

  #disconnect(): void {
    for (const unsubscribe of this.#unsubscribers) unsubscribe();
    this.#unsubscribers = [];
    this.#client?.stop();
    this.#client = null;
  }
}
