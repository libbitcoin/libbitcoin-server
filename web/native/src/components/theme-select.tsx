import { setTheme, THEMES, useTheme } from '@/lib/theme';

export function ThemeSelect() {
  const theme = useTheme();
  return (
    <label className="text-textMuted flex items-center gap-2 text-sm">
      Theme
      <select
        value={theme}
        onChange={(event) => {
          setTheme(event.target.value);
        }}
        className="border-accent/40 bg-surface text-textPrimary focus:border-accent focus:ring-accent rounded-md border py-1.5 pr-8 pl-3 text-sm"
      >
        {THEMES.map((item) => (
          <option key={item.id} value={item.id}>
            {item.label}
          </option>
        ))}
      </select>
    </label>
  );
}
