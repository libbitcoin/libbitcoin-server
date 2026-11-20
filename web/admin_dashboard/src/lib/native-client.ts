import {
  type NativeConfiguration,
  type NativeFrame,
  type NativeHeader,
  type NativeFeesSnapshot,
  type NativeTops,
  NATIVE_FEES_PATH,
  POOL_POLL_INTERVAL_MS,
  nativeBranchPath,
  parseNativeFrame,
  topHeaderOf,
} from './native-contract';
import {
  type ConnectionStatus,
  type ReconnectingWebSocket,
  type SocketChannel,
} from './reconnecting-websocket';

export interface NativeLiveState {
  configuration: NativeConfiguration | null;
  configurationReceivedAt: number | null;
  tops: NativeTops | null;
  topReceivedAt: number | null;
  branch: NativeHeader | null;
  branchReceivedAt: number | null;
  topHeader: NativeHeader | null;
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
  tops: null,
  topReceivedAt: null,
  branch: null,
  branchReceivedAt: null,
  topHeader: null,
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
        // Responses queue behind other frames on a shared socket, so any frame shows liveness.
        const baseline = Math.max(
          this.#state.topReceivedAt ?? this.#connectedAt ?? 0,
          this.#socket.getStatus().lastMessageAt ?? 0
        );
        if (baseline !== 0 && this.#now() - baseline >= NATIVE_STATE_STALE_AFTER_MS) {
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
      const { confirmed, candidate } = frame.payload;
      const branching = candidate.height > confirmed.height;
      const retained = branching && this.#state.branch?.height === confirmed.height;
      if (!branching) {
        this.#headerRequest = null;
        this.#lastHeaderRequestedAt = null;
      }
      this.#setState({
        ...this.#state,
        tops: frame.payload,
        topHeader: topHeaderOf(frame.payload),
        topReceivedAt: receivedAt,
        branch: retained ? this.#state.branch : null,
        branchReceivedAt: retained ? this.#state.branchReceivedAt : null,
      });
      if (branching) this.#requestBranch(confirmed.height, receivedAt);
      return;
    }
    if (frame.type === 'header') {
      if (frame.payload.height !== this.#headerRequest?.height) return;
      this.#headerRequest = null;
      this.#setState({
        ...this.#state,
        branch: frame.payload,
        branchReceivedAt: receivedAt,
      });
      return;
    }
    this.#setState({
      ...this.#state,
      configuration: frame.payload,
      configurationReceivedAt: receivedAt,
    });
  }

  #requestBranch(height: number, now: number): void {
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

    // Sample the branch at the polling cadence, even during rapid top announcements.
    this.#headerRequest = { height, requestedAt: now };
    this.#lastHeaderRequestedAt = now;
    if (!this.#socket.send(nativeBranchPath(height))) this.#headerRequest = null;
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
      this.#state.tops === null &&
      this.#state.topReceivedAt === null &&
      this.#state.branch === null &&
      this.#state.branchReceivedAt === null &&
      this.#state.topHeader === null &&
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
