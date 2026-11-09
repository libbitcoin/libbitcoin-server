import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useLocation, useNavigate } from 'react-router-dom';
import { useScriptExplainerPreference } from '@/app/script-explainer-preference';
import { classifySearchQuery } from '@/lib/utils/search';
import { copyTextToClipboard } from '@/lib/utils/clipboard';
import { formatHash } from '@/lib/utils/format';
import {
  getNetwork,
  getNetworkParams,
  scriptHashFromAddress,
} from '@/lib/api/providers/libbitcoin-mapper';
import {
  useBlockchainClient,
  useProviderInfo,
  useProviderSwitcher,
  useTurbo,
} from '@/lib/api/client';
import {
  addressPath,
  blockByHashPath,
  blockByHeightPath,
  blocksPath,
  dashboardPath,
  investigationsPath,
  searchPath,
  transactionOutputPath,
} from '@/lib/utils/routes';
import { rememberLibbitcoinAddressAlias } from '@/lib/utils/libbitcoin-address-alias';

type PaletteInputMode = 'default' | 'command' | 'recents';

type RecentKind = 'block' | 'tx' | 'address';

interface RecentItem {
  kind: RecentKind;
  id: string;
  path: string;
  seenAtMs: number;
  providerId: string;
}

interface PaletteItem {
  id: string;
  section: string;
  label: string;
  detail?: string;
  keywords?: string[];
  onSelect: () => void | Promise<void>;
}

const RECENTS_STORAGE_KEY = 'recentlyViewed';
const MAX_RECENTS = 50;
const NETWORK = getNetwork();
const NETWORK_PARAMS = getNetworkParams(NETWORK);

function getLocalStorage() {
  const window_ = (globalThis as unknown as { window?: Window }).window;
  if (!window_) return null;
  try {
    return window_.localStorage;
  } catch {
    return null;
  }
}

function parseRecents(raw: string | null): RecentItem[] {
  if (!raw) return [];
  try {
    const data: unknown = JSON.parse(raw);
    if (!Array.isArray(data)) return [];
    const parsed: RecentItem[] = [];
    for (const entry of data) {
      if (!entry || typeof entry !== 'object') continue;
      const record = entry as Partial<RecentItem>;
      if (record.kind !== 'block' && record.kind !== 'tx' && record.kind !== 'address') continue;
      if (typeof record.id !== 'string' || !record.id.trim()) continue;
      if (typeof record.path !== 'string' || !record.path.trim()) continue;
      if (typeof record.seenAtMs !== 'number' || !Number.isFinite(record.seenAtMs)) continue;
      if (typeof record.providerId !== 'string' || !record.providerId.trim()) continue;
      parsed.push({
        kind: record.kind,
        id: record.id,
        path: record.path,
        seenAtMs: record.seenAtMs,
        providerId: record.providerId,
      });
    }
    parsed.sort((a, b) => b.seenAtMs - a.seenAtMs);
    return parsed.slice(0, MAX_RECENTS);
  } catch {
    return [];
  }
}

function saveRecents(storage: Storage, items: RecentItem[]) {
  try {
    storage.setItem(RECENTS_STORAGE_KEY, JSON.stringify(items.slice(0, MAX_RECENTS)));
  } catch {
    // ignore storage failures
  }
}

function upsertRecent(items: RecentItem[], next: RecentItem) {
  const filtered = items.filter((item) => item.path !== next.path);
  const merged = [next, ...filtered];
  return merged.slice(0, MAX_RECENTS);
}

function deriveRecentFromPath(pathname: string): { kind: RecentKind; id: string } | null {
  const parts = pathname.split('/').filter(Boolean);
  if (parts.length < 2) return null;
  if (parts[0] !== 'v1') return null;

  if (parts[1] === 'tx' && parts.length === 3) {
    const id = decodeURIComponent(parts[2]).trim();
    return id ? { kind: 'tx', id } : null;
  }

  if (parts[1] === 'address' && parts.length === 3) {
    const id = decodeURIComponent(parts[2]).trim();
    return id ? { kind: 'address', id } : null;
  }

  if (parts[1] === 'block' && parts.length === 4) {
    const mode = parts[2];
    if (mode !== 'hash' && mode !== 'height') return null;
    const id = decodeURIComponent(parts[3]).trim();
    return id ? { kind: 'block', id } : null;
  }

  return null;
}

function labelForRecent(item: RecentItem) {
  const prefix = item.kind === 'tx' ? 'Transaction' : item.kind === 'block' ? 'Block' : 'Address';
  if (item.kind === 'address') return `${prefix} ${item.id}`;
  if (item.kind === 'block' && /^\d+$/u.test(item.id)) return `${prefix} #${item.id}`;
  return `${prefix} ${formatHash(item.id, 12)}`;
}

function matchQuery(query: string, item: Pick<PaletteItem, 'label' | 'detail' | 'keywords'>) {
  const q = query.trim().toLowerCase();
  if (!q) return true;
  const haystacks = [item.label, item.detail ?? '', ...(item.keywords ?? [])].map((value) =>
    value.toLowerCase()
  );
  return haystacks.some((value) => value.includes(q));
}

function getCurrentIdentifier(pathname: string) {
  const recent = deriveRecentFromPath(pathname);
  if (!recent) return null;
  const label = recent.kind === 'tx' ? 'TxID' : recent.kind === 'block' ? 'Block' : 'Address';
  return { kind: recent.kind, id: recent.id, label };
}

function getInputMode(query: string): PaletteInputMode {
  const trimmedStart = query.trimStart();
  if (trimmedStart.startsWith('>')) return 'command';
  if (trimmedStart.startsWith('#')) return 'recents';
  return 'default';
}

function tokenize(input: string) {
  return input
    .trim()
    .split(/\s+/u)
    .map((token) => token.trim())
    .filter(Boolean);
}

function normalizeRecentKind(token: string): RecentKind | null {
  const normalized = token.trim().toLowerCase();
  if (normalized === 'tx' || normalized === 'transaction' || normalized === 'transactions')
    return 'tx';
  if (normalized === 'block' || normalized === 'blocks') return 'block';
  if (normalized === 'address' || normalized === 'addr' || normalized === 'addresses')
    return 'address';
  return null;
}

export function CommandPalette() {
  const navigate = useNavigate();
  const location = useLocation();
  const client = useBlockchainClient();
  const providerInfo = useProviderInfo();
  const { provider, setProvider, availableProviders, selectorVisible, setSelectorVisible } =
    useProviderSwitcher();
  const { turbo, setTurbo } = useTurbo();
  const { enabled: scriptExplainerEnabled, setEnabled: setScriptExplainerEnabled } =
    useScriptExplainerPreference();

  const storage = useMemo(() => getLocalStorage(), []);
  const [recents, setRecents] = useState<RecentItem[]>(() =>
    storage ? parseRecents(storage.getItem(RECENTS_STORAGE_KEY)) : []
  );

  const [isOpen, setIsOpen] = useState(false);
  const [query, setQuery] = useState('');
  const [activeIndex, setActiveIndex] = useState(0);
  const [statusMessage, setStatusMessage] = useState<string | null>(null);
  const restoreFocusRef = useRef<HTMLElement | null>(null);

  const showStatus = useCallback((message: string) => {
    setStatusMessage(message);
  }, []);

  const openPalette = useCallback(() => {
    const document_ = (globalThis as unknown as { document?: Document }).document;
    restoreFocusRef.current = (document_?.activeElement as HTMLElement | null) ?? null;
    setIsOpen(true);
    setQuery('');
    setActiveIndex(0);
    setStatusMessage(null);
  }, []);

  const closePalette = useCallback(() => {
    setIsOpen(false);
    const target = restoreFocusRef.current;
    restoreFocusRef.current = null;
    if (!target) return;
    globalThis.setTimeout(() => {
      target.focus();
    }, 0);
  }, []);

  const dismissPalette = useCallback(() => {
    setIsOpen(false);
  }, []);

  const inputRef = useCallback((node: HTMLInputElement | null) => {
    if (!node) return;
    node.focus();
    node.select();
  }, []);

  useEffect(() => {
    if (!statusMessage) return;
    const timeout = setTimeout(() => {
      setStatusMessage(null);
    }, 1500);
    return () => {
      clearTimeout(timeout);
    };
  }, [statusMessage]);

  const currentIdentifier = useMemo(
    () => getCurrentIdentifier(location.pathname),
    [location.pathname]
  );

  useEffect(() => {
    if (!storage) return;
    const derived = deriveRecentFromPath(location.pathname);
    if (!derived) return;
    const next: RecentItem = {
      kind: derived.kind,
      id: derived.id,
      path: location.pathname,
      seenAtMs: Date.now(),
      providerId: providerInfo.id,
    };
    setRecents((prev) => {
      const merged = upsertRecent(prev, next);
      saveRecents(storage, merged);
      return merged;
    });
  }, [location.pathname, providerInfo.id, storage]);

  useEffect(() => {
    const window_ = (globalThis as unknown as { window?: Window }).window;
    if (!window_) return;

    const onKeyDown = (event: KeyboardEvent) => {
      if (!event.key) return;
      const key = event.key.toLowerCase();
      const isCommandK = (event.metaKey || event.ctrlKey) && key === 'k';
      if (!isCommandK) return;

      event.preventDefault();
      if (isOpen) {
        closePalette();
      } else {
        openPalette();
      }
    };

    window_.addEventListener('keydown', onKeyDown);
    return () => {
      window_.removeEventListener('keydown', onKeyDown);
    };
  }, [closePalette, isOpen, openPalette]);

  if (!isOpen) return null;

  const classification = classifySearchQuery(query);

  const items: PaletteItem[] = (() => {
    const inputMode = getInputMode(query);
    const trimmedStart = query.trimStart();

    if (inputMode === 'command') {
      const raw = trimmedStart.slice(1);
      const tokens = tokenize(raw);
      const commandToken = (tokens[0] ?? '').toLowerCase();
      const args = tokens.slice(1);
      const argsText = args.join(' ').trim();

      const commandSuggestions: PaletteItem[] = [
        {
          id: 'cmd.provider',
          section: 'Commands',
          label: 'provider — switch provider',
          keywords: ['provider', 'switch'],
          onSelect: () => {
            setQuery('>provider ');
          },
        },
        {
          id: 'cmd.turbo',
          section: 'Commands',
          label: 'turbo — set turbo on/off',
          keywords: ['turbo', 'on', 'off', 'toggle'],
          onSelect: () => {
            setQuery('>turbo ');
          },
        },
        {
          id: 'cmd.script',
          section: 'Commands',
          label: 'script — set script explainer on/off',
          keywords: ['script', 'explainer', 'on', 'off', 'toggle'],
          onSelect: () => {
            setQuery('>script ');
          },
        },
        {
          id: 'cmd.copy',
          section: 'Commands',
          label: 'copy — copy url/id/endpoint',
          keywords: ['copy', 'clipboard', 'url', 'endpoint', 'id'],
          onSelect: () => {
            setQuery('>copy ');
          },
        },
        {
          id: 'cmd.nav',
          section: 'Commands',
          label: 'nav — navigate pages',
          keywords: ['nav', 'navigate', 'dashboard', 'blocks'],
          onSelect: () => {
            setQuery('>nav ');
          },
        },
        {
          id: 'cmd.recents',
          section: 'Commands',
          label: 'recents — browse recently viewed (#)',
          keywords: ['recent', 'history', '#'],
          onSelect: () => {
            setQuery('#');
          },
        },
      ];

      if (!commandToken) {
        return commandSuggestions;
      }

      const knownCommands = new Set(['provider', 'turbo', 'script', 'copy', 'nav']);
      if (!knownCommands.has(commandToken)) {
        const matches = commandSuggestions.filter((item) =>
          matchQuery(commandToken, {
            label: item.label,
            detail: item.detail,
            keywords: item.keywords,
          })
        );
        return matches.length > 0
          ? matches
          : [
              {
                id: 'cmd.unknown',
                section: 'Commands',
                label: `Unknown command: ${commandToken}`,
                detail: 'Try: provider, turbo, script, copy, nav',
                onSelect: () => {
                  setQuery('>');
                },
              },
            ];
      }

      if (commandToken === 'provider') {
        const normalizedArg = argsText.toLowerCase();
        const switchItems = availableProviders.map((p) => {
          const label = p === 'libbitcoin' ? 'server' : p;
          const isActive = p === provider;
          return {
            id: `cmd.provider.${p}`,
            section: 'Provider',
            label: `Switch provider to ${label}${isActive ? ' (current)' : ''}`,
            keywords: ['provider', p, label],
            onSelect: () => {
              dismissPalette();
              setProvider(p);
            },
          } satisfies PaletteItem;
        });
        const selectorToggle: PaletteItem = {
          id: 'cmd.provider.selector',
          section: 'Provider',
          label: selectorVisible
            ? 'Hide provider selector in header'
            : 'Show provider selector in header',
          keywords: ['provider', 'selector', 'dropdown', 'header', 'show', 'hide', 'toggle'],
          onSelect: () => {
            dismissPalette();
            setSelectorVisible(!selectorVisible);
          },
        };
        const matches = [...switchItems, selectorToggle].filter((item) => {
          if (!normalizedArg) return true;
          return matchQuery(normalizedArg, item);
        });

        return matches.length > 0
          ? matches
          : [
              {
                id: 'cmd.provider.none',
                section: 'Provider',
                label: 'No provider matches',
                detail: argsText || '—',
                onSelect: () => {},
              },
            ];
      }

      if (commandToken === 'turbo') {
        if (provider !== 'libbitcoin') {
          return [
            {
              id: 'cmd.turbo.unavailable',
              section: 'Provider',
              label: 'Turbo is only available on the server provider',
              detail: `Current provider: ${provider}`,
              onSelect: () => {
                showStatus('Turbo is only available when provider is server');
              },
            },
          ];
        }

        const normalized = argsText.toLowerCase();
        const options: PaletteItem[] = [
          {
            id: 'cmd.turbo.on',
            section: 'Provider',
            label: `Turbo on${turbo ? ' (current)' : ''}`,
            keywords: ['turbo', 'on'],
            onSelect: () => {
              dismissPalette();
              setTurbo(true);
            },
          },
          {
            id: 'cmd.turbo.off',
            section: 'Provider',
            label: `Turbo off${turbo ? '' : ' (current)'}`,
            keywords: ['turbo', 'off'],
            onSelect: () => {
              dismissPalette();
              setTurbo(false);
            },
          },
          {
            id: 'cmd.turbo.toggle',
            section: 'Provider',
            label: `Turbo toggle (${turbo ? 'on' : 'off'})`,
            keywords: ['turbo', 'toggle'],
            onSelect: () => {
              dismissPalette();
              setTurbo(!turbo);
            },
          },
        ];

        return normalized ? options.filter((item) => matchQuery(normalized, item)) : options;
      }

      if (commandToken === 'script') {
        const options: PaletteItem[] = [
          {
            id: 'cmd.script.on',
            section: 'Features',
            label: `Script explainer on${scriptExplainerEnabled ? ' (current)' : ''}`,
            keywords: ['on', 'enable', 'show'],
            onSelect: () => {
              dismissPalette();
              setScriptExplainerEnabled(true);
            },
          },
          {
            id: 'cmd.script.off',
            section: 'Features',
            label: `Script explainer off${scriptExplainerEnabled ? '' : ' (current)'}`,
            keywords: ['off', 'disable', 'hide'],
            onSelect: () => {
              dismissPalette();
              setScriptExplainerEnabled(false);
            },
          },
          {
            id: 'cmd.script.toggle',
            section: 'Features',
            label: `Toggle script explainer (${scriptExplainerEnabled ? 'on' : 'off'})`,
            keywords: ['toggle'],
            onSelect: () => {
              dismissPalette();
              setScriptExplainerEnabled(!scriptExplainerEnabled);
            },
          },
        ];
        return argsText ? options.filter((item) => matchQuery(argsText, item)) : options;
      }

      if (commandToken === 'copy') {
        const normalized = argsText.toLowerCase();
        const clipboardActions: PaletteItem[] = [
          {
            id: 'cmd.copy.url',
            section: 'Clipboard',
            label: 'Copy current URL',
            keywords: ['copy', 'url', 'link'],
            onSelect: async () => {
              const window_ = (globalThis as unknown as { window?: Window }).window;
              const url = window_?.location.href;
              if (!url) return;
              const ok = await copyTextToClipboard(url);
              showStatus(ok ? 'Copied URL to clipboard' : 'Failed to copy URL');
            },
          },
          ...(currentIdentifier?.id
            ? [
                {
                  id: 'cmd.copy.identifier',
                  section: 'Clipboard',
                  label: `Copy ${currentIdentifier.label}`,
                  detail: currentIdentifier.id,
                  keywords: ['copy', currentIdentifier.kind, currentIdentifier.label.toLowerCase()],
                  onSelect: async () => {
                    const ok = await copyTextToClipboard(currentIdentifier.id);
                    showStatus(
                      ok
                        ? `Copied ${currentIdentifier.label}`
                        : `Failed to copy ${currentIdentifier.label}`
                    );
                  },
                } satisfies PaletteItem,
              ]
            : [
                {
                  id: 'cmd.copy.identifier.unavailable',
                  section: 'Clipboard',
                  label: 'Copy identifier (unavailable on this page)',
                  keywords: ['copy', 'id', 'identifier'],
                  onSelect: () => {
                    showStatus('No identifier on this page');
                  },
                },
              ]),
          ...(providerInfo.endpoint
            ? [
                {
                  id: 'cmd.copy.endpoint',
                  section: 'Clipboard',
                  label: 'Copy provider endpoint',
                  detail: providerInfo.endpoint,
                  keywords: ['copy', 'endpoint', 'api'],
                  onSelect: async () => {
                    const ok = await copyTextToClipboard(providerInfo.endpoint ?? '');
                    showStatus(ok ? 'Copied endpoint' : 'Failed to copy endpoint');
                  },
                } satisfies PaletteItem,
              ]
            : []),
        ];

        return normalized
          ? clipboardActions.filter((item) => matchQuery(normalized, item))
          : clipboardActions;
      }

      if (commandToken === 'nav') {
        const normalized = argsText.toLowerCase();
        const navActions: PaletteItem[] = [
          {
            id: 'cmd.nav.investigations',
            section: 'Navigation',
            label: 'Open saved investigations',
            keywords: ['investigations', 'saved', 'bookmarks', 'notes'],
            onSelect: () => {
              dismissPalette();
              void navigate(investigationsPath());
            },
          },
          {
            id: 'cmd.nav.dashboard',
            section: 'Navigation',
            label: 'Go to Dashboard',
            keywords: ['dashboard', 'home'],
            onSelect: () => {
              dismissPalette();
              void navigate(dashboardPath());
            },
          },
          {
            id: 'cmd.nav.blocks',
            section: 'Navigation',
            label: 'Go to Blocks',
            keywords: ['blocks', 'block list'],
            onSelect: () => {
              dismissPalette();
              void navigate(blocksPath());
            },
          },
          {
            id: 'cmd.nav.latestBlock',
            section: 'Navigation',
            label: 'Go to Latest Block',
            keywords: ['latest', 'top'],
            onSelect: async () => {
              dismissPalette();
              try {
                const header = await client.getBlockHeader('latest');
                void navigate(blockByHashPath(header.hash));
              } catch (error) {
                console.warn('Failed to load latest block', error);
                showStatus('Failed to load latest block');
              }
            },
          },
        ];

        return normalized ? navActions.filter((item) => matchQuery(normalized, item)) : navActions;
      }

      return commandSuggestions;
    }

    if (inputMode === 'recents') {
      const raw = trimmedStart.slice(1);
      const tokens = tokenize(raw);
      const first = tokens[0]?.toLowerCase() ?? '';

      const clearRecentsAction: PaletteItem = {
        id: 'recent.clear',
        section: 'History',
        label: 'Clear recently viewed',
        keywords: ['recent', 'history', 'clear'],
        onSelect: () => {
          if (!storage) {
            setRecents([]);
            return;
          }
          setRecents([]);
          try {
            storage.removeItem(RECENTS_STORAGE_KEY);
          } catch {
            // ignore
          }
        },
      };

      if (first === 'clear') {
        return [clearRecentsAction];
      }

      let kindFilter: RecentKind | null = null;
      const kind = tokens[0] ? normalizeRecentKind(tokens[0]) : null;
      let filterTokens = tokens;
      if (kind) {
        kindFilter = kind;
        filterTokens = tokens.slice(1);
      }
      const filterText = filterTokens.join(' ').trim().toLowerCase();

      const recentItems = recents
        .filter((item) => {
          if (kindFilter && item.kind !== kindFilter) return false;
          if (!filterText) return true;
          const id = item.id.toLowerCase();
          const providerId = item.providerId.toLowerCase();
          const path = item.path.toLowerCase();
          return (
            id.includes(filterText) || providerId.includes(filterText) || path.includes(filterText)
          );
        })
        .slice(0, 25)
        .map(
          (item) =>
            ({
              id: `recent.${item.kind}.${item.path}`,
              section: 'Recently Viewed',
              label: labelForRecent(item),
              detail: item.providerId,
              keywords: [item.kind, item.id, item.providerId],
              onSelect: () => {
                dismissPalette();
                void navigate(item.path);
              },
            }) satisfies PaletteItem
        );

      const backAction: PaletteItem = {
        id: 'recent.back',
        section: 'History',
        label: 'Back to all commands',
        keywords: ['back'],
        onSelect: () => {
          setQuery('');
        },
      };

      return recentItems.length > 0
        ? [...recentItems, clearRecentsAction, backAction]
        : [backAction, clearRecentsAction];
    }

    const trimmed = query.trim();

    const baseActions: PaletteItem[] = [
      {
        id: 'nav.investigations',
        section: 'Navigation',
        label: 'Open saved investigations',
        keywords: ['investigations', 'saved', 'bookmarks', 'notes'],
        onSelect: () => {
          dismissPalette();
          void navigate(investigationsPath());
        },
      },
      {
        id: 'nav.dashboard',
        section: 'Navigation',
        label: 'Go to Dashboard',
        keywords: ['dashboard', 'home'],
        onSelect: () => {
          dismissPalette();
          void navigate(dashboardPath());
        },
      },
      {
        id: 'nav.blocks',
        section: 'Navigation',
        label: 'Go to Blocks',
        keywords: ['blocks', 'block list'],
        onSelect: () => {
          dismissPalette();
          void navigate(blocksPath());
        },
      },
      {
        id: 'nav.latestBlock',
        section: 'Navigation',
        label: 'Go to Latest Block',
        keywords: ['latest', 'top'],
        onSelect: async () => {
          dismissPalette();
          try {
            const header = await client.getBlockHeader('latest');
            void navigate(blockByHashPath(header.hash));
          } catch (error) {
            console.warn('Failed to load latest block', error);
            showStatus('Failed to load latest block');
          }
        },
      },
      {
        id: 'recent.open',
        section: 'History',
        label: 'Show recently viewed (#)',
        keywords: ['recent', 'history', 'visited', '#'],
        onSelect: () => {
          setQuery('#');
        },
      },
      {
        id: 'cmd.open',
        section: 'Commands',
        label: 'Command mode (>)',
        keywords: ['command', '>'],
        onSelect: () => {
          setQuery('>');
        },
      },
    ];

    const providerActions: PaletteItem[] = [
      ...availableProviders.map((p) => {
        const label = p === 'libbitcoin' ? 'server' : p;
        const isActive = p === provider;
        return {
          id: `provider.${p}`,
          section: 'Provider',
          label: `Switch provider to ${label}${isActive ? ' (current)' : ''}`,
          keywords: ['provider', 'switch', p, label],
          detail: providerInfo.endpoint,
          onSelect: () => {
            dismissPalette();
            setProvider(p);
          },
        } satisfies PaletteItem;
      }),
      {
        id: 'provider.selector',
        section: 'Provider',
        label: selectorVisible
          ? 'Hide provider selector in header'
          : 'Show provider selector in header',
        keywords: ['provider', 'selector', 'dropdown', 'header', 'show', 'hide', 'toggle'],
        onSelect: () => {
          dismissPalette();
          setSelectorVisible(!selectorVisible);
        },
      },
    ];

    const turboActions: PaletteItem[] =
      provider === 'libbitcoin'
        ? [
            {
              id: 'turbo.toggle',
              section: 'Provider',
              label: `Toggle Turbo (${turbo ? 'on' : 'off'})`,
              keywords: ['turbo', 'fast', 'libbitcoin'],
              onSelect: () => {
                dismissPalette();
                setTurbo(!turbo);
              },
            },
          ]
        : [];

    const clipboardActions: PaletteItem[] = [
      {
        id: 'copy.url',
        section: 'Clipboard',
        label: 'Copy current URL',
        keywords: ['copy', 'url', 'link', 'share'],
        onSelect: async () => {
          const window_ = (globalThis as unknown as { window?: Window }).window;
          const url = window_?.location.href;
          if (!url) return;
          const ok = await copyTextToClipboard(url);
          showStatus(ok ? 'Copied URL to clipboard' : 'Failed to copy URL');
        },
      },
      ...(currentIdentifier?.id
        ? [
            {
              id: 'copy.identifier',
              section: 'Clipboard',
              label: `Copy ${currentIdentifier.label}`,
              detail: currentIdentifier.id,
              keywords: ['copy', currentIdentifier.kind, currentIdentifier.label.toLowerCase()],
              onSelect: async () => {
                const ok = await copyTextToClipboard(currentIdentifier.id);
                showStatus(
                  ok
                    ? `Copied ${currentIdentifier.label}`
                    : `Failed to copy ${currentIdentifier.label}`
                );
              },
            } satisfies PaletteItem,
          ]
        : []),
      ...(providerInfo.endpoint
        ? [
            {
              id: 'copy.endpoint',
              section: 'Clipboard',
              label: 'Copy provider endpoint',
              detail: providerInfo.endpoint,
              keywords: ['copy', 'endpoint', 'api', 'provider', 'url'],
              onSelect: async () => {
                const ok = await copyTextToClipboard(providerInfo.endpoint ?? '');
                showStatus(ok ? 'Copied endpoint' : 'Failed to copy endpoint');
              },
            } satisfies PaletteItem,
          ]
        : []),
    ];

    const recentActions: PaletteItem[] = recents.slice(0, 10).map((item) => ({
      id: `recent.${item.kind}.${item.path}`,
      section: 'Recently Viewed',
      label: labelForRecent(item),
      detail: item.providerId,
      keywords: [item.kind, item.id, item.providerId],
      onSelect: () => {
        dismissPalette();
        void navigate(item.path);
      },
    }));

    const clearRecentsAction: PaletteItem = {
      id: 'recent.clear',
      section: 'History',
      label: 'Clear recently viewed',
      keywords: ['recent', 'history', 'clear'],
      onSelect: () => {
        if (!storage) {
          setRecents([]);
          return;
        }
        setRecents([]);
        try {
          storage.removeItem(RECENTS_STORAGE_KEY);
        } catch {
          // ignore
        }
      },
    };

    const goToActions: PaletteItem[] = (() => {
      if (!trimmed || classification.type === 'unknown' || !classification.value) return [];
      const value = classification.value;
      switch (classification.type) {
        case 'outpoint': {
          const outputIndex = classification.outputIndex;
          if (outputIndex === undefined) return [];
          return [
            {
              id: 'goto.outpoint',
              section: 'Go To',
              label: `Go to output ${String(outputIndex)} of ${formatHash(value, 12)}`,
              detail: `${value}:${String(outputIndex)}`,
              keywords: ['go', 'goto', 'output', 'outpoint', 'transaction', 'txid'],
              onSelect: () => {
                dismissPalette();
                void navigate(transactionOutputPath(value, outputIndex));
              },
            },
          ];
        }
        case 'block-height': {
          return [
            {
              id: 'goto.blockHeight',
              section: 'Go To',
              label: `Go to block #${value}`,
              keywords: ['go', 'goto', 'block', 'height'],
              onSelect: () => {
                dismissPalette();
                void navigate(blockByHeightPath(value));
              },
            },
          ];
        }
        case 'hash': {
          return [
            {
              id: 'goto.hash',
              section: 'Go To',
              label: `Look up block or transaction ${formatHash(value, 12)}`,
              detail: value,
              keywords: ['go', 'goto', 'block', 'hash', 'transaction', 'txid'],
              onSelect: () => {
                dismissPalette();
                void navigate(searchPath(value));
              },
            },
          ];
        }
        case 'address': {
          return [
            {
              id: 'goto.address',
              section: 'Go To',
              label: `Go to address ${value}`,
              keywords: ['go', 'goto', 'address'],
              onSelect: () => {
                dismissPalette();
                const isLibbitcoin = providerInfo.id === 'libbitcoin';
                const scriptInfo = isLibbitcoin
                  ? scriptHashFromAddress(value, NETWORK_PARAMS)
                  : null;
                const scriptHash = scriptInfo?.hash ?? null;
                if (isLibbitcoin && scriptHash) {
                  rememberLibbitcoinAddressAlias(NETWORK, scriptHash, value);
                }
                void navigate(addressPath(scriptHash ?? value), {
                  state: isLibbitcoin ? { displayAddress: value } : undefined,
                });
              },
            },
          ];
        }
        default: {
          return [];
        }
      }
    })();

    const searchAction =
      trimmed && classification.type === 'unknown'
        ? {
            id: 'goto.search',
            section: 'Go To',
            label: `Search for “${trimmed}”`,
            keywords: ['search'],
            onSelect: () => {
              dismissPalette();
              void navigate(searchPath(trimmed));
            },
          }
        : null;

    const all = [
      ...goToActions,
      ...baseActions,
      ...providerActions,
      ...turboActions,
      {
        id: 'script.toggle',
        section: 'Features',
        label: scriptExplainerEnabled ? 'Disable script explainer' : 'Enable script explainer',
        detail: 'Explain scripts in transaction input and output details',
        keywords: ['script', 'explainer', 'opcode', 'witness', 'show', 'hide', 'toggle'],
        onSelect: () => {
          dismissPalette();
          setScriptExplainerEnabled(!scriptExplainerEnabled);
        },
      },
      ...clipboardActions,
      ...recentActions,
      clearRecentsAction,
      ...(searchAction ? [searchAction] : []),
    ];

    return all.filter((item) => matchQuery(query, item));
  })();

  const clampedActiveIndex = items.length > 0 ? Math.min(activeIndex, items.length - 1) : 0;
  const activeItem = items.at(clampedActiveIndex) ?? null;

  return (
    <div
      className="fixed inset-0 z-50 flex items-start justify-center p-4 pt-24"
      role="dialog"
      aria-modal="true"
      aria-label="Command palette"
    >
      <div
        className="absolute inset-0 bg-black/60 backdrop-blur-sm"
        aria-hidden
        onMouseDown={() => {
          closePalette();
        }}
      />
      <div className="border-accent/30 bg-surface/95 relative w-full max-w-xl overflow-hidden rounded-2xl border shadow-2xl">
        <div className="border-accentSecondary/20 flex items-center gap-3 border-b px-4 py-3">
          <input
            ref={inputRef}
            value={query}
            onChange={(event) => {
              setQuery(event.target.value);
              setActiveIndex(0);
            }}
            onKeyDown={(event) => {
              if (event.key === 'Escape') {
                event.preventDefault();
                closePalette();
                return;
              }
              if (event.key === 'ArrowDown') {
                event.preventDefault();
                if (items.length === 0) return;
                setActiveIndex((prev) => {
                  const next = Math.min(prev, items.length - 1) + 1;
                  return Math.min(items.length - 1, next);
                });
                return;
              }
              if (event.key === 'ArrowUp') {
                event.preventDefault();
                if (items.length === 0) return;
                setActiveIndex((prev) => Math.max(0, Math.min(prev, items.length - 1) - 1));
                return;
              }
              if (event.key === 'Enter') {
                event.preventDefault();
                if (activeItem) {
                  void activeItem.onSelect();
                }
              }
            }}
            placeholder={
              getInputMode(query) === 'command'
                ? 'Commands: >provider, >turbo, >script, >copy, >nav'
                : getInputMode(query) === 'recents'
                  ? 'Recents: # tx | # block | # address'
                  : 'Type a command or paste an identifier…'
            }
            className="text-textPrimary placeholder:text-textMuted w-full bg-transparent text-sm outline-none"
            aria-label="Command input"
          />
          <kbd className="text-textMuted hidden rounded border border-white/10 bg-black/30 px-2 py-1 text-[10px] tracking-wide uppercase sm:block">
            Esc
          </kbd>
        </div>

        <div className="max-h-[60vh] overflow-y-auto">
          {items.length === 0 ? (
            <p className="text-textMuted px-4 py-6 text-sm">No matches.</p>
          ) : (
            <div className="py-2">
              <div className="space-y-1 px-2 pb-2">
                {items.map((item, index) => {
                  const isActive = index === clampedActiveIndex;
                  const previousSection = items[index - 1]?.section;
                  const showSectionHeader = previousSection !== item.section;
                  return (
                    <div key={item.id}>
                      {showSectionHeader ? (
                        <p className="text-accentSecondary px-2 py-2 text-[10px] font-semibold tracking-wide uppercase">
                          {item.section}
                        </p>
                      ) : null}
                      <button
                        type="button"
                        onMouseDown={(event) => {
                          event.preventDefault();
                        }}
                        onMouseEnter={() => {
                          setActiveIndex(index);
                        }}
                        onClick={() => {
                          void item.onSelect();
                        }}
                        className={`flex w-full items-start justify-between gap-3 rounded-lg px-3 py-2 text-left text-sm ${
                          isActive
                            ? 'bg-accentSecondary/20 text-textPrimary'
                            : 'text-textPrimary hover:bg-accentSecondary/10'
                        }`}
                      >
                        <span className="min-w-0 flex-1">
                          <span className="block truncate">{item.label}</span>
                          {item.detail ? (
                            <span className="text-textMuted block truncate text-xs">
                              {item.detail}
                            </span>
                          ) : null}
                        </span>
                      </button>
                    </div>
                  );
                })}
              </div>
            </div>
          )}
        </div>

        <div className="border-accentSecondary/20 flex items-center justify-between gap-3 border-t px-4 py-3 text-xs">
          <p className="text-textMuted">
            <span className="hidden sm:inline">↑↓ to navigate · Enter to run · </span>
            Ctrl/Cmd+K to close
          </p>
          {statusMessage ? <p className="text-accent">{statusMessage}</p> : null}
        </div>
      </div>
    </div>
  );
}
