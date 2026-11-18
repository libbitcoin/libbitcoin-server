import React from 'react';
import ReactDOM from 'react-dom/client';
import { App } from '@/app/app';
import { AppProviders } from '@/app/providers';
import './index.css';
import { initializeTheme } from '@/lib/theme';

initializeTheme();

// The site is a single page, so any other address is shown as the root.
if (window.location.pathname !== '/' || window.location.search || window.location.hash)
  window.history.replaceState(null, '', '/');

const rootElement = document.querySelector('#root');
if (!rootElement) {
  throw new Error('Root element not found');
}

ReactDOM.createRoot(rootElement).render(
  <React.StrictMode>
    <AppProviders>
      <App />
    </AppProviders>
  </React.StrictMode>
);
