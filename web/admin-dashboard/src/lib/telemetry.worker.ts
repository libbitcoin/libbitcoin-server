import { TelemetryWorkerRuntime } from './telemetry-worker-runtime';
import type { TelemetryCommand, TelemetryResponse } from './telemetry-worker-protocol';

// Use the small shared surface of Window/Worker types without combining lib.dom
// and lib.webworker globals in the application's TypeScript configuration.
const workerScope = globalThis as unknown as {
  addEventListener: (
    type: 'message',
    listener: (event: MessageEvent<TelemetryCommand>) => void
  ) => void;
  postMessage: (message: TelemetryResponse) => void;
  close: () => void;
};
const runtime = new TelemetryWorkerRuntime((message) => {
  workerScope.postMessage(message);
});
workerScope.addEventListener('message', (event) => {
  runtime.handle(event.data);
  if (event.data.type === 'stop') workerScope.close();
});
