import { useServerConfiguration } from '@/lib/hooks/useBlockchainData';
import { usePageTitle } from '@/lib/hooks/usePageTitle';

export function ConfigurationPage() {
  usePageTitle('Configuration');
  const { data: configuration, isLoading, isError, error } = useServerConfiguration();

  return (
    <div className="space-y-8">
      <header>
        <h1 tabIndex={-1} className="text-accent text-2xl font-semibold">
          Configuration
        </h1>
        <p className="text-textMuted text-sm">The configuration served by this node.</p>
      </header>

      {isError && (
        <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
          <p className="text-accent font-semibold">Configuration unavailable</p>
          <p className="text-textMuted mt-1 break-words">
            {error instanceof Error ? error.message : 'Unknown error'}
          </p>
        </div>
      )}
      {isLoading && <div className="text-textMuted py-10 text-sm">Loading configuration...</div>}

      {configuration && (
        <pre className="border-accent/30 bg-surface/80 text-textPrimary overflow-x-auto rounded-2xl border p-5 text-sm">
          {JSON.stringify(configuration, null, 2)}
        </pre>
      )}
    </div>
  );
}
