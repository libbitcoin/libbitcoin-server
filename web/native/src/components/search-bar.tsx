import { type SyntheticEvent, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { MagnifyingGlassIcon } from '@heroicons/react/24/outline';
import { searchPath } from '@/lib/utils/routes';

export function SearchBar() {
  const [value, setValue] = useState('');
  const navigate = useNavigate();

  const onSubmit = (event: SyntheticEvent<HTMLFormElement>) => {
    event.preventDefault();
    const trimmed = value.trim();
    if (!trimmed) return;
    void navigate(searchPath(trimmed));
  };

  return (
    <form onSubmit={onSubmit} className="relative w-full md:w-auto">
      <MagnifyingGlassIcon
        className="text-accent pointer-events-none absolute top-1/2 left-3 h-4 w-4 -translate-y-1/2"
        aria-hidden="true"
      />
      <input
        type="search"
        aria-label="Search blocks, transactions, and addresses"
        placeholder="Search blocks, transactions, addresses"
        title="Search by block height, hash, address, or outpoint (txid:vout)"
        className="border-accent/40 bg-surface text-textPrimary placeholder:text-textMuted focus:border-accent focus:ring-accent w-full rounded-md border py-1.5 pr-3 pl-9 text-sm md:w-[19rem]"
        value={value}
        onChange={(event) => {
          setValue(event.target.value);
        }}
      />
    </form>
  );
}
