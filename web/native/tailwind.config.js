import forms from '@tailwindcss/forms';

export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: {
    extend: {
      colors: {
        background: 'var(--theme-background)',
        surface: 'var(--theme-surface)',
        accent: 'var(--theme-accent)',
        accentSecondary: 'var(--theme-accent-secondary)',
        textPrimary: 'var(--theme-text-primary)',
        textMuted: 'var(--theme-text-muted)',
        inset: 'var(--theme-inset)',
        noticeSuccess: 'var(--theme-notice-success)',
        noticeWarning: 'var(--theme-notice-warning)',
        noticeText: 'var(--theme-notice-text)',
      },
    },
  },
  plugins: [forms],
};
