// A choice of one of several options, as a row of buttons.
export function Toggle<Value extends string>({
  label,
  options,
  value,
  onChange,
}: {
  label: string;
  options: ReadonlyArray<{ id: Value; label: string }>;
  value: Value;
  onChange: (value: Value) => void;
}) {
  return (
    <div role="group" aria-label={label} className="flex">
      {options.map((option, index) => (
        <button
          key={option.id}
          type="button"
          aria-pressed={option.id === value}
          onClick={() => {
            onChange(option.id);
          }}
          className={`border px-3 py-2 font-medium ${index > 0 ? '-ml-px' : 'rounded-l-md'} ${
            index === options.length - 1 ? 'rounded-r-md' : ''
          } ${
            option.id === value
              ? 'border-accent/40 bg-accentSecondary/20 text-accent'
              : 'border-accent/30 text-textMuted hover:text-accent hover:bg-accentSecondary/10'
          }`}
        >
          {option.label}
        </button>
      ))}
    </div>
  );
}
