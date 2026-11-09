import { createContext, type ReactNode, useContext, useEffect, useState } from 'react';

const STORAGE_KEY = 'scriptExplainerEnabled';
const ScriptExplainerPreferenceContext = createContext<{
  enabled: boolean;
  setEnabled: (enabled: boolean) => void;
} | null>(null);

export function ScriptExplainerPreferenceProvider({ children }: { children: ReactNode }) {
  const [enabled, setEnabled] = useState(() => {
    try {
      return globalThis.localStorage.getItem(STORAGE_KEY) === 'true';
    } catch {
      return false;
    }
  });

  useEffect(() => {
    try {
      globalThis.localStorage.setItem(STORAGE_KEY, String(enabled));
    } catch {
      // The preference still works for this session when storage is unavailable.
    }
  }, [enabled]);

  return (
    <ScriptExplainerPreferenceContext.Provider value={{ enabled, setEnabled }}>
      {children}
    </ScriptExplainerPreferenceContext.Provider>
  );
}

export function useScriptExplainerPreference() {
  const preference = useContext(ScriptExplainerPreferenceContext);
  if (!preference) throw new Error('ScriptExplainerPreferenceProvider missing in component tree');
  return preference;
}
