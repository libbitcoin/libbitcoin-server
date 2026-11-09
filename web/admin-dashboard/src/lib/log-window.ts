import { getLogLevel, includesLogLevel } from './admin-contract';
import type { RecentLog } from './live-metrics';
import { STREAM_WINDOW_LIMIT } from './stream-window';

export interface LogQuery {
  search: string;
  filter: number;
  offset: number;
}

export interface LogWindow {
  logs: readonly RecentLog[];
  retained: number;
  matched: number;
  offset: number;
}

export function selectLogWindow(logs: readonly RecentLog[], query: LogQuery): LogWindow {
  const search = query.search.trim().toLowerCase();
  const matches = logs.filter(
    (log) =>
      includesLogLevel(query.filter, log.level) &&
      (!search ||
        log.message.toLowerCase().includes(search) ||
        getLogLevel(log.level).name.includes(search))
  );
  const requested = Number.isFinite(query.offset) ? Math.max(0, Math.floor(query.offset)) : 0;
  const offset = Math.min(requested, Math.max(0, matches.length - STREAM_WINDOW_LIMIT));
  return {
    logs: matches.slice(offset, offset + STREAM_WINDOW_LIMIT),
    retained: logs.length,
    matched: matches.length,
    offset,
  };
}
