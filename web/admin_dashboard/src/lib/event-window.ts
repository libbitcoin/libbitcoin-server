import { getEventDefinition, type EventCategory } from './admin-contract';
import type { RecentEvent } from './live-metrics';
import { STREAM_WINDOW_LIMIT } from './stream-window';

export interface EventQuery {
  search: string;
  category: EventCategory | 'all';
  offset: number;
}

export interface EventWindow {
  events: readonly RecentEvent[];
  retained: number;
  matched: number;
  offset: number;
}

export function selectEventWindow(events: readonly RecentEvent[], query: EventQuery): EventWindow {
  const search = query.search.trim().toLowerCase();
  const matches = events.filter((event) => {
    const definition = getEventDefinition(event.event);
    return (
      (query.category === 'all' || definition.category === query.category) &&
      (!search ||
        definition.name.includes(search) ||
        definition.label.toLowerCase().includes(search) ||
        String(event.value).includes(search))
    );
  });
  const requested = Number.isFinite(query.offset) ? Math.max(0, Math.floor(query.offset)) : 0;
  const offset = Math.min(requested, Math.max(0, matches.length - STREAM_WINDOW_LIMIT));
  return {
    events: matches.slice(offset, offset + STREAM_WINDOW_LIMIT),
    retained: events.length,
    matched: matches.length,
    offset,
  };
}
