import { Link, useRouteError } from 'react-router-dom';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { dashboardPath } from '@/lib/utils/routes';

export function NotFoundPage() {
  usePageTitle('Page Not Found');
  const error = useRouteError() as Error | undefined;
  const message = error?.message ?? 'This page could not be found.';

  return (
    <div className="text-textPrimary space-y-4 py-10">
      <h1 tabIndex={-1} className="text-accent text-2xl font-semibold">
        Page not found
      </h1>
      <p className="text-textMuted">{message}</p>
      <Link to={dashboardPath()} className="text-accent hover:text-accentSecondary">
        Return to dashboard
      </Link>
    </div>
  );
}
