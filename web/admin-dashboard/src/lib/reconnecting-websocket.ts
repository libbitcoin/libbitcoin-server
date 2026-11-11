export type ConnectionPhase = 'idle' | 'connecting' | 'connected' | 'reconnecting';

export interface ConnectionStatus {
  phase: ConnectionPhase;
  attempt: number;
  reconnects: number;
  connectedAt: number | null;
  lastMessageAt: number | null;
  error: string | null;
}

type StatusListener = () => void;

// One of several clients sharing a socket. Each frame is offered to the
// channels in attachment order until one claims it.
export interface SocketChannel {
  onOpen?: (socket: WebSocket) => void;
  onMessage: (text: string) => boolean;
  onUnclaimed?: (text: string) => void;
  onHeartbeat?: (socket: WebSocket) => void;
  heartbeatMs?: number;
  onBeforeStop?: (socket: WebSocket) => void;
}

export interface ReconnectingWebSocketOptions {
  url: string;
  webSocketFactory?: (url: string) => WebSocket;
  connectTimeoutMs?: number;
  retryMinimumMs?: number;
  retryMaximumMs?: number;
  jitterRatio?: number;
  now?: () => number;
  random?: () => number;
}

const INITIAL_STATUS: ConnectionStatus = {
  phase: 'idle',
  attempt: 0,
  reconnects: 0,
  connectedAt: null,
  lastMessageAt: null,
  error: null,
};

export class ReconnectingWebSocket {
  readonly #options: Required<
    Pick<
      ReconnectingWebSocketOptions,
      | 'connectTimeoutMs'
      | 'retryMinimumMs'
      | 'retryMaximumMs'
      | 'jitterRatio'
      | 'now'
      | 'random'
      | 'webSocketFactory'
    >
  > &
    ReconnectingWebSocketOptions;

  #status: ConnectionStatus = INITIAL_STATUS;
  #socket: WebSocket | null = null;
  #listeners = new Set<StatusListener>();
  #connectTimer: ReturnType<typeof setTimeout> | null = null;
  #retryTimer: ReturnType<typeof setTimeout> | null = null;
  #channels: SocketChannel[] = [];
  #heartbeatTimers = new Map<SocketChannel, ReturnType<typeof setInterval>>();
  #retryDelayMs: number;
  #started = false;

  constructor(options: ReconnectingWebSocketOptions) {
    this.#options = {
      ...options,
      connectTimeoutMs: options.connectTimeoutMs ?? 5_000,
      retryMinimumMs: options.retryMinimumMs ?? 1_000,
      retryMaximumMs: options.retryMaximumMs ?? 30_000,
      jitterRatio: options.jitterRatio ?? 0.2,
      now: options.now ?? Date.now,
      random: options.random ?? Math.random,
      webSocketFactory: options.webSocketFactory ?? ((url) => new WebSocket(url)),
    };
    this.#retryDelayMs = this.#options.retryMinimumMs;
  }

  readonly getStatus = (): ConnectionStatus => this.#status;

  readonly subscribeStatus = (listener: StatusListener): (() => void) => {
    this.#listeners.add(listener);
    return () => {
      this.#listeners.delete(listener);
    };
  };

  attach(channel: SocketChannel): () => void {
    this.#channels.push(channel);
    const socket = this.#socket;
    if (socket?.readyState === 1 && this.#status.phase === 'connected') {
      channel.onOpen?.(socket);
      this.#startHeartbeat(socket, channel);
    }
    return () => {
      const index = this.#channels.indexOf(channel);
      if (index < 0) return;
      this.#channels.splice(index, 1);
      this.#clearHeartbeat(channel);
      const current = this.#socket;
      if (current?.readyState === 1) {
        try {
          channel.onBeforeStop?.(current);
        } catch {
          // The socket is already closing.
        }
      }
    };
  }

  start(): void {
    if (this.#started) return;
    this.#started = true;
    this.#retryDelayMs = this.#options.retryMinimumMs;
    this.#connect();
  }

  stop(): void {
    this.#started = false;
    this.#clearTimers();
    const socket = this.#socket;
    this.#socket = null;
    if (socket) {
      try {
        if (socket.readyState === 1)
          for (const channel of [...this.#channels]) channel.onBeforeStop?.(socket);
        socket.close();
      } catch {
        // The socket is already closing.
      }
    }
    this.#setStatus(INITIAL_STATUS);
  }

  send(text: string): boolean {
    if (this.#socket?.readyState !== 1) return false;
    try {
      this.#socket.send(text);
      return true;
    } catch {
      return false;
    }
  }

  #connect(): void {
    if (!this.#started || this.#socket) return;

    const reconnecting = this.#status.reconnects > 0;
    this.#setStatus({
      ...this.#status,
      phase: reconnecting ? 'reconnecting' : 'connecting',
      attempt: this.#status.attempt + 1,
      error: null,
    });

    let socket: WebSocket;
    try {
      socket = this.#options.webSocketFactory(this.#options.url);
      this.#socket = socket;
    } catch (error) {
      this.#setStatus({ ...this.#status, error: errorMessage(error) });
      this.#scheduleRetry();
      return;
    }

    this.#connectTimer = setTimeout(() => {
      if (this.#socket !== socket) return;
      this.#setStatus({ ...this.#status, error: 'Connection timed out' });
      try {
        socket.close();
      } catch {
        this.#socket = null;
        this.#scheduleRetry();
      }
    }, this.#options.connectTimeoutMs);

    socket.addEventListener('open', () => {
      if (this.#socket !== socket || !this.#started) return;
      this.#clearConnectTimer();
      this.#retryDelayMs = this.#options.retryMinimumMs;
      this.#setStatus({
        ...this.#status,
        phase: 'connected',
        connectedAt: this.#options.now(),
        error: null,
      });
      for (const channel of [...this.#channels]) {
        if (this.#socket !== socket) return;
        channel.onOpen?.(socket);
        this.#startHeartbeat(socket, channel);
      }
    });

    socket.addEventListener('message', (event) => {
      if (this.#socket !== socket || !this.#started) return;
      const text = messageText(event.data);
      if (text === null) return;

      const now = this.#options.now();
      if (this.#status.lastMessageAt === null || now - this.#status.lastMessageAt >= 1_000) {
        this.#setStatus({ ...this.#status, lastMessageAt: now });
      }
      const channels = [...this.#channels];
      if (channels.some((channel) => channel.onMessage(text))) return;
      for (const channel of channels) channel.onUnclaimed?.(text);
    });

    socket.addEventListener('error', () => {
      if (this.#socket !== socket || !this.#started) return;
      this.#setStatus({ ...this.#status, error: 'WebSocket connection failed' });
      try {
        socket.close();
      } catch {
        this.#socket = null;
        this.#scheduleRetry();
      }
    });

    socket.addEventListener('close', () => {
      if (this.#socket !== socket) return;
      this.#socket = null;
      this.#clearConnectTimer();
      this.#clearHeartbeats();
      if (this.#started) this.#scheduleRetry();
    });
  }

  #scheduleRetry(): void {
    if (!this.#started || this.#retryTimer) return;
    const jitter = 1 + (this.#options.random() * 2 - 1) * this.#options.jitterRatio;
    const delay = Math.max(0, Math.round(this.#retryDelayMs * jitter));
    this.#retryDelayMs = Math.min(this.#retryDelayMs * 2, this.#options.retryMaximumMs);
    this.#setStatus({
      ...this.#status,
      phase: 'reconnecting',
      reconnects: this.#status.reconnects + 1,
      connectedAt: null,
    });
    this.#retryTimer = setTimeout(() => {
      this.#retryTimer = null;
      this.#connect();
    }, delay);
  }

  #startHeartbeat(socket: WebSocket, channel: SocketChannel): void {
    this.#clearHeartbeat(channel);
    const heartbeatMs = channel.heartbeatMs ?? 240_000;
    if (!channel.onHeartbeat || heartbeatMs <= 0) return;
    const timer = setInterval(() => {
      if (this.#socket === socket && socket.readyState === 1) {
        channel.onHeartbeat?.(socket);
      }
    }, heartbeatMs);
    this.#heartbeatTimers.set(channel, timer);
  }

  #clearConnectTimer(): void {
    if (!this.#connectTimer) return;
    clearTimeout(this.#connectTimer);
    this.#connectTimer = null;
  }

  #clearHeartbeat(channel: SocketChannel): void {
    const timer = this.#heartbeatTimers.get(channel);
    if (timer === undefined) return;
    clearInterval(timer);
    this.#heartbeatTimers.delete(channel);
  }

  #clearHeartbeats(): void {
    for (const timer of this.#heartbeatTimers.values()) clearInterval(timer);
    this.#heartbeatTimers.clear();
  }

  #clearTimers(): void {
    this.#clearConnectTimer();
    this.#clearHeartbeats();
    if (this.#retryTimer) {
      clearTimeout(this.#retryTimer);
      this.#retryTimer = null;
    }
  }

  #setStatus(status: ConnectionStatus): void {
    this.#status = status;
    for (const listener of this.#listeners) listener();
  }
}

function messageText(value: unknown): string | null {
  if (typeof value === 'string') return value;
  if (value instanceof ArrayBuffer) return new TextDecoder().decode(value);
  return null;
}

function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : 'Unable to create WebSocket';
}
