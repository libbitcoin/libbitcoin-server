import {
  ALL_EVENT_FILTER,
  DEFAULT_LOG_FILTER,
  type AdminFrame,
  includesLogLevel,
  isAdminLogFrame,
  parseAdminFrame,
} from './admin-contract';
import {
  type ConnectionStatus,
  type ReconnectingWebSocket,
  type SocketChannel,
} from './reconnecting-websocket';

type FrameListener = (frame: AdminFrame) => void;
type InvalidFrameListener = (text: string) => void;

interface AdminClientOptions {
  logFilter?: number;
  eventFilter?: number;
}

export class AdminClient {
  #logFilter: number;
  #eventFilter: number;
  #frameListeners = new Set<FrameListener>();
  #invalidFrameListeners = new Set<InvalidFrameListener>();
  #socket: ReconnectingWebSocket;
  #channel: SocketChannel;
  #detach: (() => void) | null = null;

  constructor(socket: ReconnectingWebSocket, options: AdminClientOptions = {}) {
    this.#logFilter = options.logFilter ?? DEFAULT_LOG_FILTER;
    this.#eventFilter = options.eventFilter ?? ALL_EVENT_FILTER;
    this.#socket = socket;
    this.#channel = {
      onOpen: (socket) => {
        this.#sendSubscriptions(socket);
      },
      onHeartbeat: (socket) => {
        this.#sendSubscriptions(socket);
      },
      onBeforeStop: (socket) => {
        socket.send('/v1/log/subscribe?filter=0');
        socket.send('/v1/event/subscribe?filter=0');
      },
      onMessage: (text) => {
        const frame = parseAdminFrame(text);
        if (!frame) return false;
        // A few old frames may already be in flight after reducing the server mask.
        if (isAdminLogFrame(frame) && !includesLogLevel(this.#logFilter, frame.level)) return true;
        for (const listener of this.#frameListeners) listener(frame);
        return true;
      },
      onUnclaimed: (text) => {
        for (const listener of this.#invalidFrameListeners) listener(text);
      },
    };
  }

  readonly getStatus = (): ConnectionStatus => this.#socket.getStatus();

  readonly subscribeStatus = (listener: () => void): (() => void) =>
    this.#socket.subscribeStatus(listener);

  start(): void {
    this.#detach ??= this.#socket.attach(this.#channel);
  }

  stop(): void {
    this.#detach?.();
    this.#detach = null;
  }

  subscribeFrames(listener: FrameListener): () => void {
    this.#frameListeners.add(listener);
    return () => {
      this.#frameListeners.delete(listener);
    };
  }

  subscribeInvalidFrames(listener: InvalidFrameListener): () => void {
    this.#invalidFrameListeners.add(listener);
    return () => {
      this.#invalidFrameListeners.delete(listener);
    };
  }

  setFilters(logFilter: number, eventFilter: number): void {
    if (this.#logFilter !== logFilter) {
      this.#logFilter = logFilter;
      this.#socket.send(`/v1/log/subscribe?filter=${String(logFilter)}`);
    }
    if (this.#eventFilter !== eventFilter) {
      this.#eventFilter = eventFilter;
      this.#socket.send(`/v1/event/subscribe?filter=${String(eventFilter)}`);
    }
  }

  #sendSubscriptions(socket: WebSocket): void {
    socket.send(`/v1/log/subscribe?filter=${String(this.#logFilter)}`);
    socket.send(`/v1/event/subscribe?filter=${String(this.#eventFilter)}`);
  }
}
