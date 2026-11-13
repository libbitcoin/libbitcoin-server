export interface ScriptLookupStatusProps {
  hex?: string;
  loading?: boolean;
  error?: string | null;
  onRetry?: () => void;
}

export function ScriptLookupStatus({ hex, loading, error, onRetry }: ScriptLookupStatusProps) {
  return (
    <>
      {loading && hex === undefined && (
        <p role="status" className="text-textMuted">
          Loading script bytes…
        </p>
      )}
      {error && (
        <div role="alert" className="text-accentSecondary space-y-2 break-words">
          <p>Could not load script bytes: {error}</p>
          {onRetry && (
            <button
              type="button"
              disabled={loading}
              onClick={onRetry}
              className="text-accent underline disabled:opacity-50"
            >
              {loading ? 'Retrying…' : 'Retry script lookup'}
            </button>
          )}
        </div>
      )}
      {!loading && !error && hex === undefined && (
        <p className="text-textMuted">Script bytes were not reported.</p>
      )}
    </>
  );
}
