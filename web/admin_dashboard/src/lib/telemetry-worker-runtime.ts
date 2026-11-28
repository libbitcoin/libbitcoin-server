import { AdminClient } from './admin-client';
import { ALL_EVENT_FILTER, isAdminMiningFrame, isFirstHeaderFrame } from './admin-contract';
import { toWebSocketUrl } from './endpoints';
import { NativeClient } from './native-client';
import { ReconnectingWebSocket } from './reconnecting-websocket';
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

type SocketFactory = (endpoint: string) => ReconnectingWebSocket;

// No DOM or React dependencies: the worker owns receipt, validation and aggregation.
export class TelemetryWorkerRuntime {
  #metrics: LiveMetricsStore;
  #client: AdminClient | null = null;
  #native: NativeClient | null = null;
  #sockets: ReconnectingWebSocket[] = [];
  #endpoint: string | null = null;
  #nativeEndpoint: string | null = null;
  #generation = 0;
  #previous: MetricsSnapshot | null = null;
  #frozenLogs: readonly RecentLog[] | null = null;
  #frozenEvents: readonly RecentEvent[] | null = null;
  #unsubscribers: Array<() => void> = [];
  #candidate: number | null = null;

  constructor(
    readonly send: (message: TelemetryResponse) => void,
    readonly createSocket: SocketFactory = (endpoint) =>
      new ReconnectingWebSocket({ url: toWebSocketUrl(endpoint) }),
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
      if (
        command.endpoint !== this.#endpoint ||
        command.nativeEndpoint !== this.#nativeEndpoint ||
        this.#client === null
      ) {
        this.#connect(command.endpoint, command.nativeEndpoint, command.logFilter);
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
        this.#client?.setPaused(command.stream, command.paused);
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
      case 'clear-faults': {
        this.#metrics.clearFaults();
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

  // Admin and native share one socket when they share an endpoint.
  #connect(endpoint: string, nativeEndpoint: string, logFilter: number): void {
    this.#disconnect();
    this.#endpoint = endpoint;
    this.#nativeEndpoint = nativeEndpoint;
    const adminSocket = this.createSocket(endpoint);
    const nativeSocket =
      nativeEndpoint === endpoint ? adminSocket : this.createSocket(nativeEndpoint);
    this.#sockets = adminSocket === nativeSocket ? [adminSocket] : [adminSocket, nativeSocket];
    const client = new AdminClient(adminSocket, { logFilter });
    const native = new NativeClient(nativeSocket);
    let awaitingTop = true;
    const sendNative = () => {
      const status = native.getStatus();
      const live = native.getState();
      if (status.phase !== 'connected') awaitingTop = true;
      else if (live.tops !== null) {
        // A candidate top below the last seen on reconnect is a new node run.
        const candidate = live.tops.candidate.height;
        if (awaitingTop && this.#candidate !== null && candidate < this.#candidate) {
          this.#reset(this.#generation);
          for (const socket of this.#sockets) socket.resetReconnects();
        }
        awaitingTop = false;
        this.#candidate = candidate;
      }
      this.send({ type: 'native', status, live });
    };
    this.#client = client;
    this.#native = native;
    let connected = false;
    this.#unsubscribers = [
      client.subscribeStatus(() => {
        const current = client.getStatus().phase === 'connected';
        if (current && !connected) this.#reset(this.#generation);
        connected = current;
      }),
      client.subscribeFrames((frame) => {
        if (isAdminMiningFrame(frame)) {
          this.send({ type: 'mining', generation: this.#generation, mining: frame });
          return;
        }

        if (isFirstHeaderFrame(frame)) this.#reset(this.#generation);
        this.#metrics.ingest(frame);
      }),
      client.subscribeInvalidFrames(() => {
        this.#metrics.recordInvalidFrame();
      }),
      native.subscribeStatus(sendNative),
      native.subscribeState(sendNative),
    ];
    client.start();
    native.start();
    for (const socket of this.#sockets) socket.start();
  }

  #disconnect(): void {
    for (const unsubscribe of this.#unsubscribers) unsubscribe();
    this.#unsubscribers = [];
    for (const socket of this.#sockets) socket.stop();
    this.#client?.stop();
    this.#native?.stop();
    this.#sockets = [];
    this.#client = null;
    this.#native = null;
  }
}
