import forms from '@tailwindcss/forms';

export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: {
    extend: {
      colors: {
        background: '#272822',
        surface: '#1f211c',
        accent: '#ffa022',
        accentSecondary: '#c85637',
        textPrimary: '#ededed',
        textMuted: '#cfcfcf',
      },
    },
  },
  plugins: [forms],
};
