// A loading indicator, with what is loading.
export function Spinner({ label }: { label: string }) {
  return (
    <span role="status" className="text-textMuted inline-flex items-center gap-2 text-sm">
      <span
        aria-hidden="true"
        className="border-accent/30 border-t-accent h-4 w-4 shrink-0 animate-spin rounded-full border-2 motion-reduce:animate-none"
      />
      {label}
    </span>
  );
}
