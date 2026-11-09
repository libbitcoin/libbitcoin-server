import { useQuery } from '@tanstack/react-query';
import { useEffect, useMemo, useSyncExternalStore } from 'react';
import { DEFAULT_LOG_FILTER } from './admin-contract';
import { isSameOriginEndpoint } from './endpoints';
import type { ConnectionStatus } from './reconnecting-websocket';
import type { WorkerMetricsStore } from './worker-metrics-store';
import {
  NATIVE_STATE_STALE_AFTER_MS,
  NATIVE_POLL_INTERVAL_MS,
  NativeClient,
  type NativeLiveState,
} from './native-client';
import {
  fetchNativeSnapshot,
  fetchNativeFees,
  POOL_POLL_INTERVAL_MS,
  POOL_STALE_AFTER_MS,
  type NativeSnapshot,
  type NativeFeesSnapshot,
} from './native-contract';

export interface NativeTelemetry {
  status: ConnectionStatus;
  live: NativeLiveState;
  top: number | null;
  topHeader: NativeLiveState['topHeader'];
  configuration: NativeLiveState['configuration'];
  receivedAt: number | null;
  transport: 'websocket' | 'rest' | 'none';
  restError: string | null;
}

type NativeTelemetrySelection = Pick<
  NativeTelemetry,
  'top' | 'topHeader' | 'configuration' | 'receivedAt' | 'transport'
>;

export function useAdminTelemetry(
  endpoint: string,
  metrics: WorkerMetricsStore,
  logFilter = DEFAULT_LOG_FILTER
): ConnectionStatus {
  const status = useSyncExternalStore(
    metrics.subscribeStatus,
    metrics.getStatus,
    metrics.getStatus
  );

  // Apply preferences before start and update the existing socket on control changes.
  useEffect(() => {
    // This synchronizes the external server subscription, not React parent state.
    metrics.configure(endpoint, logFilter);
  }, [endpoint, metrics, logFilter]);

  useEffect(() => {
    metrics.start();

    return () => {
      metrics.stop();
    };
  }, [metrics]);

  return status;
}

export function useNativeTelemetry(
  endpoint: string,
  metrics: Pick<WorkerMetricsStore, 'recordNativeTop' | 'recordNativeFees'>,
  now: number
): NativeTelemetry {
  const client = useMemo(() => new NativeClient(endpoint), [endpoint]);
  const canUseRestFallback = useMemo(() => isSameOriginEndpoint(endpoint), [endpoint]);
  const status = useSyncExternalStore(client.subscribeStatus, client.getStatus, client.getStatus);
  const live = useSyncExternalStore(client.subscribeState, client.getState, client.getState);
  const liveIsFresh = hasFreshNativeLiveState(status, live, now);

  useEffect(() => {
    client.start();
    return () => {
      client.stop();
    };
  }, [client]);

  const rest = useQuery({
    queryKey: ['native-snapshot', endpoint],
    queryFn: ({ signal }) => fetchNativeSnapshot(endpoint, signal),
    enabled: canUseRestFallback && !liveIsFresh,
    refetchInterval: NATIVE_POLL_INTERVAL_MS,
    retry: false,
  });

  const selection = selectNativeTelemetrySource(status, live, rest.data ?? null, now);
  const liveFees = selectNativeFeesSource(status, live, null, now);
  const restFees = useQuery({
    queryKey: ['native-fees', endpoint],
    queryFn: ({ signal }) => fetchNativeFees(endpoint, signal),
    enabled: canUseRestFallback && liveFees === null,
    refetchInterval: POOL_POLL_INTERVAL_MS,
    retry: false,
  });
  const fees = selectNativeFeesSource(status, live, restFees.data ?? null, now);

  useEffect(() => {
    if (fees !== null) {
      // eslint-disable-next-line react-you-might-not-need-an-effect/no-pass-data-to-parent
      metrics.recordNativeFees(fees.data, fees.receivedAt);
    }
  }, [fees, metrics]);

  // The transport owns canonical top updates; mirror them into the external
  // metrics store so the chart can retain a bounded session history.
  useEffect(() => {
    if (selection.top !== null) {
      // eslint-disable-next-line react-you-might-not-need-an-effect/no-pass-data-to-parent
      metrics.recordNativeTop(selection.top, selection.receivedAt ?? Date.now());
    }
  }, [metrics, selection.receivedAt, selection.top]);

  return {
    status,
    live,
    ...selection,
    restError: rest.error instanceof Error ? rest.error.message : null,
  };
}

export function hasFreshNativeLiveState(
  status: ConnectionStatus,
  live: NativeLiveState,
  now: number
): boolean {
  return (
    status.phase === 'connected' &&
    live.top !== null &&
    live.topReceivedAt !== null &&
    live.configuration !== null &&
    live.configurationReceivedAt !== null &&
    isNativeStateFresh(live.topReceivedAt, now)
  );
}

export function selectNativeTelemetrySource(
  status: ConnectionStatus,
  live: NativeLiveState,
  rest: NativeSnapshot | null,
  now: number
): NativeTelemetrySelection {
  if (hasFreshNativeLiveState(status, live, now)) {
    return {
      top: live.top,
      topHeader:
        live.headerReceivedAt !== null && isNativeStateFresh(live.headerReceivedAt, now)
          ? live.topHeader
          : null,
      configuration: live.configuration,
      receivedAt: live.topReceivedAt,
      transport: 'websocket',
    };
  }

  if (rest && isNativeStateFresh(rest.receivedAt, now)) {
    return {
      top: rest.top,
      topHeader: rest.topHeader,
      configuration: rest.configuration,
      receivedAt: rest.receivedAt,
      transport: 'rest',
    };
  }

  return {
    top: null,
    topHeader: null,
    configuration: null,
    receivedAt: null,
    transport: 'none',
  };
}

export function selectNativeFeesSource(
  status: ConnectionStatus,
  live: NativeLiveState,
  rest: NativeFeesSnapshot | null,
  now: number
): NativeFeesSnapshot | null {
  if (
    status.phase === 'connected' &&
    live.fees !== null &&
    now - live.fees.receivedAt < POOL_STALE_AFTER_MS
  )
    return live.fees;
  return rest !== null && now - rest.receivedAt < POOL_STALE_AFTER_MS ? rest : null;
}

function isNativeStateFresh(receivedAt: number, now: number): boolean {
  return now - receivedAt <= NATIVE_STATE_STALE_AFTER_MS;
}
