import {
  type NativeConfiguration,
  type NativeFrame,
  type NativeHeader,
  type NativeFeesSnapshot,
  NATIVE_FEES_PATH,
  POOL_POLL_INTERVAL_MS,
  nativeHeaderPath,
  parseNativeFrame,
} from './native-contract';
import {
  type ConnectionStatus,
  type ReconnectingWebSocket,
  type SocketChannel,
} from './reconnecting-websocket';

export interface NativeLiveState {
  configuration: NativeConfiguration | null;
  configurationReceivedAt: number | null;
  top: number | null;
  topReceivedAt: number | null;
  topHeader: NativeHeader | null;
  headerReceivedAt: number | null;
  fees: NativeFeesSnapshot | null;
  invalidFrames: number;
}

interface NativeClientOptions {
  pollIntervalMs?: number;
  now?: () => number;
}

export const NATIVE_POLL_INTERVAL_MS = 15_000;
export const NATIVE_STATE_STALE_AFTER_MS = 3 * NATIVE_POLL_INTERVAL_MS;

export const INITIAL_LIVE_STATE: NativeLiveState = {
  configuration: null,
  configurationReceivedAt: null,
  top: null,
  topReceivedAt: null,
  topHeader: null,
  headerReceivedAt: null,
  fees: null,
  invalidFrames: 0,
};

export class NativeClient {
  #state: NativeLiveState = INITIAL_LIVE_STATE;
  #stateListeners = new Set<() => void>();
  #socket: ReconnectingWebSocket;
  #channel: SocketChannel;
  #detach: (() => void) | null = null;
  #unsubscribeStatus: (() => void) | null = null;
  #now: () => number;
  #connectedAt: number | null = null;
  #headerRequest: { height: number; requestedAt: number } | null = null;
  #lastHeaderRequestedAt: number | null = null;
  #pollIntervalMs: number;
  #poolTimer: ReturnType<typeof setInterval> | null = null;
  #poolRequestedAt: number | null = null;

  constructor(socket: ReconnectingWebSocket, options: NativeClientOptions = {}) {
    this.#now = options.now ?? Date.now;
    this.#pollIntervalMs = options.pollIntervalMs ?? NATIVE_POLL_INTERVAL_MS;
    this.#socket = socket;
    this.#channel = {
      heartbeatMs: this.#pollIntervalMs,
      onOpen: (socket) => {
        this.#connectedAt = this.#now();
        socket.send('/v1/configuration?format=json');
        socket.send('/v1/top/subscribe?format=json');
        this.#poolRequestedAt = null;
        this.#requestPool();
      },
      onHeartbeat: (socket) => {
        const topBaseline = this.#state.topReceivedAt ?? this.#connectedAt;
        if (topBaseline !== null && this.#now() - topBaseline >= NATIVE_STATE_STALE_AFTER_MS) {
          try {
            socket.close(4000, 'Native top response timed out');
          } catch {
            // The freshness gate still prevents stale state from being selected.
          }
          return;
        }
        socket.send('/v1/top?format=json');
      },
      onBeforeStop: (socket) => {
        socket.send('/v1/top/subscribe?format=json&stop=true');
      },
      onMessage: (text) => {
        const frame = parseNativeFrame(text);
        if (!frame) return false;
        this.#applyFrame(frame);
        return true;
      },
      onUnclaimed: () => {
        this.#setState({ ...this.#state, invalidFrames: this.#state.invalidFrames + 1 });
      },
    };
  }

  readonly getStatus = (): ConnectionStatus => this.#socket.getStatus();

  readonly subscribeStatus = (listener: () => void): (() => void) =>
    this.#socket.subscribeStatus(listener);

  readonly getState = (): NativeLiveState => this.#state;

  readonly subscribeState = (listener: () => void): (() => void) => {
    this.#stateListeners.add(listener);
    return () => {
      this.#stateListeners.delete(listener);
    };
  };

  start(): void {
    this.#poolTimer ??= setInterval(() => {
      this.#requestPool();
    }, POOL_POLL_INTERVAL_MS);
    this.#unsubscribeStatus ??= this.#socket.subscribeStatus(() => {
      if (this.#socket.getStatus().phase !== 'connected') {
        this.#connectedAt = null;
        this.#poolRequestedAt = null;
        this.#headerRequest = null;
        this.#lastHeaderRequestedAt = null;
        this.#invalidateLiveState();
      }
    });
    this.#detach ??= this.#socket.attach(this.#channel);
  }

  stop(): void {
    if (this.#poolTimer !== null) clearInterval(this.#poolTimer);
    this.#poolTimer = null;
    this.#poolRequestedAt = null;
    this.#unsubscribeStatus?.();
    this.#unsubscribeStatus = null;
    this.#detach?.();
    this.#detach = null;
  }

  #applyFrame(frame: NativeFrame): void {
    const receivedAt = this.#now();
    if (frame.type === 'fees') {
      this.#poolRequestedAt = null;
      this.#setState({ ...this.#state, fees: { data: frame.payload, receivedAt } });
      return;
    }
    if (frame.type === 'top') {
      const reorganized = this.#state.top !== null && frame.payload < this.#state.top;
      if (reorganized) {
        this.#headerRequest = null;
        this.#lastHeaderRequestedAt = null;
      }
      this.#setState({
        ...this.#state,
        top: frame.payload,
        topReceivedAt: receivedAt,
        topHeader: reorganized ? null : this.#state.topHeader,
        headerReceivedAt: reorganized ? null : this.#state.headerReceivedAt,
      });
      this.#requestHeader(frame.payload, receivedAt);
      return;
    }
    if (frame.type === 'header') {
      if (frame.payload.height !== this.#headerRequest?.height) return;
      this.#headerRequest = null;
      this.#setState({
        ...this.#state,
        topHeader: frame.payload,
        headerReceivedAt: receivedAt,
      });
      return;
    }
    this.#setState({
      ...this.#state,
      configuration: frame.payload,
      configurationReceivedAt: receivedAt,
    });
  }

  #requestHeader(height: number, now: number): void {
    if (
      this.#headerRequest !== null &&
      now - this.#headerRequest.requestedAt < NATIVE_STATE_STALE_AFTER_MS
    )
      return;
    if (
      this.#lastHeaderRequestedAt !== null &&
      now - this.#lastHeaderRequestedAt < this.#pollIntervalMs
    )
      return;

    // Sample headers at the polling cadence, even during rapid top announcements.
    this.#headerRequest = { height, requestedAt: now };
    this.#lastHeaderRequestedAt = now;
    if (!this.#socket.send(nativeHeaderPath(height))) this.#headerRequest = null;
  }

  #requestPool(): void {
    const now = this.#now();
    // Avoid piling up snapshot queries; retry an unanswered request after 30 seconds.
    if (this.#poolRequestedAt !== null && now - this.#poolRequestedAt < 30_000) return;
    if (this.#socket.send(NATIVE_FEES_PATH)) this.#poolRequestedAt = now;
  }

  #invalidateLiveState(): void {
    if (
      this.#state.configuration === null &&
      this.#state.configurationReceivedAt === null &&
      this.#state.top === null &&
      this.#state.topReceivedAt === null &&
      this.#state.topHeader === null &&
      this.#state.headerReceivedAt === null &&
      this.#state.fees === null
    ) {
      return;
    }

    this.#setState({ ...INITIAL_LIVE_STATE, invalidFrames: this.#state.invalidFrames });
  }

  #setState(state: NativeLiveState): void {
    this.#state = state;
    for (const listener of this.#stateListeners) listener();
  }
}
