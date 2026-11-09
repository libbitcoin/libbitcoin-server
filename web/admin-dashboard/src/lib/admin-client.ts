import {
  ALL_EVENT_FILTER,
  DEFAULT_LOG_FILTER,
  type AdminFrame,
  includesLogLevel,
  isAdminLogFrame,
  parseAdminFrame,
} from './admin-contract';
import { toWebSocketUrl } from './endpoints';
import { type ConnectionStatus, ReconnectingWebSocket } from './reconnecting-websocket';

type FrameListener = (frame: AdminFrame) => void;
type InvalidFrameListener = (text: string) => void;

interface AdminClientOptions {
  logFilter?: number;
  eventFilter?: number;
  webSocketFactory?: (url: string) => WebSocket;
  retryMinimumMs?: number;
  retryMaximumMs?: number;
  jitterRatio?: number;
}

export class AdminClient {
  #logFilter: number;
  #eventFilter: number;
  #frameListeners = new Set<FrameListener>();
  #invalidFrameListeners = new Set<InvalidFrameListener>();
  #socket: ReconnectingWebSocket;

  constructor(baseUrl: string, options: AdminClientOptions = {}) {
    this.#logFilter = options.logFilter ?? DEFAULT_LOG_FILTER;
    this.#eventFilter = options.eventFilter ?? ALL_EVENT_FILTER;
    this.#socket = new ReconnectingWebSocket({
      url: toWebSocketUrl(baseUrl),
      webSocketFactory: options.webSocketFactory,
      retryMinimumMs: options.retryMinimumMs,
      retryMaximumMs: options.retryMaximumMs,
      jitterRatio: options.jitterRatio,
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
        if (!frame) {
          for (const listener of this.#invalidFrameListeners) listener(text);
          return;
        }
        // A few old frames may already be in flight after reducing the server mask.
        if (isAdminLogFrame(frame) && !includesLogLevel(this.#logFilter, frame.level)) return;
        for (const listener of this.#frameListeners) listener(frame);
      },
    });
  }

  readonly getStatus = (): ConnectionStatus => this.#socket.getStatus();

  readonly subscribeStatus = (listener: () => void): (() => void) =>
    this.#socket.subscribeStatus(listener);

  start(): void {
    this.#socket.start();
  }

  stop(): void {
    this.#socket.stop();
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
