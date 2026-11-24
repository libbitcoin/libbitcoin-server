import { useMemo, useState, type ReactNode } from 'react';
import { ChevronDownIcon, ChevronUpDownIcon, ChevronUpIcon } from '@heroicons/react/24/outline';

export type SortDirection = 'ascending' | 'descending';

export interface Sort<Key extends string> {
  key: Key;
  direction: SortDirection;
}

type Value = number | string | undefined | null;

// Rows sorted by the chosen column, else in their given order. A missing value
// sorts last in either direction. Values are read by getters fixed per table.
export function useSortedRows<Row, Key extends string>(
  rows: Row[],
  values: Record<Key, (row: Row) => Value>
) {
  const [sort, setSort] = useState<Sort<Key> | null>(null);
  const sorted = useMemo(() => {
    if (!sort) return rows;
    const value = values[sort.key];
    const sign = sort.direction === 'ascending' ? 1 : -1;
    return rows
      .map((row, index) => ({ row, index, value: value(row) }))
      .sort((left, right) => {
        if (left.value == null || right.value == null) {
          if (left.value == null && right.value == null) return left.index - right.index;
          return left.value == null ? 1 : -1;
        }
        const order =
          typeof left.value === 'number' && typeof right.value === 'number'
            ? left.value - right.value
            : String(left.value).localeCompare(String(right.value));
        return order === 0 ? left.index - right.index : sign * order;
      })
      .map((entry) => entry.row);
  }, [rows, sort, values]);

  // Each click on a column cycles ascending, descending, then given order.
  const toggle = (key: Key) => {
    setSort((current) => {
      if (current?.key !== key) return { key, direction: 'ascending' };
      if (current.direction === 'ascending') return { key, direction: 'descending' };
      return null;
    });
  };

  return { sorted, sort, toggle };
}

export function SortHeader<Key extends string>({
  label,
  column,
  sort,
  onSort,
  align = 'left',
}: {
  label: ReactNode;
  column: Key;
  sort: Sort<Key> | null;
  onSort: (key: Key) => void;
  align?: 'left' | 'right';
}) {
  const direction = sort?.key === column ? sort.direction : undefined;
  return (
    <th
      aria-sort={direction ?? 'none'}
      className={`px-4 py-3 ${align === 'right' ? 'text-right' : 'text-left'}`}
    >
      <button
        type="button"
        onClick={() => {
          onSort(column);
        }}
        className={`hover:text-accent inline-flex items-center gap-1 uppercase ${
          align === 'right' ? 'flex-row-reverse' : ''
        } ${direction ? 'text-accent' : ''}`}
      >
        <span>{label}</span>
        <SortIcon direction={direction} />
      </button>
    </th>
  );
}

function SortIcon({ direction }: { direction?: SortDirection }) {
  const className = 'h-3.5 w-3.5 shrink-0';
  if (direction === 'ascending') return <ChevronUpIcon aria-hidden="true" strokeWidth={2.5} className={className} />;
  if (direction === 'descending') return <ChevronDownIcon aria-hidden="true" strokeWidth={2.5} className={className} />;
  return <ChevronUpDownIcon aria-hidden="true" strokeWidth={2} className={`${className} opacity-40`} />;
}
