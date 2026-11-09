import forms from '@tailwindcss/forms';

export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: {
    extend: {
      // Match the explorer explicitly, independent of Tailwind's default font stack.
      fontFamily: {
        sans: [
          'ui-sans-serif',
          'system-ui',
          'sans-serif',
          '"Apple Color Emoji"',
          '"Segoe UI Emoji"',
          '"Segoe UI Symbol"',
          '"Noto Color Emoji"',
        ],
      },
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
