import React from 'react';
import ReactDOM from 'react-dom/client';
import './index.css';
import { AppProviders } from './app/providers';
import { AppRouter } from './app/router';

const rootElement = document.querySelector('#root');
if (!rootElement) {
  throw new Error('Root element not found');
}

ReactDOM.createRoot(rootElement).render(
  <React.StrictMode>
    <AppProviders>
      <AppRouter />
    </AppProviders>
  </React.StrictMode>
);
