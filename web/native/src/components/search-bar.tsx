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
    <form
      onSubmit={onSubmit}
      className="border-accent/30 bg-surface/80 focus-within:border-accent flex w-full items-center gap-2 rounded-xl border px-3 py-2 text-sm shadow-inner md:w-auto"
    >
      <MagnifyingGlassIcon className="text-accent h-4 w-4" aria-hidden="true" />
      <input
        type="search"
        aria-label="Search blocks, transactions, and addresses"
        placeholder="Search blocks, transactions, addresses"
        title="Search by block height, hash, address, or outpoint (txid:vout)"
        className="text-textPrimary placeholder:text-textMuted w-full bg-transparent focus:outline-none md:w-72"
        value={value}
        onChange={(event) => {
          setValue(event.target.value);
        }}
      />
    </form>
  );
}
