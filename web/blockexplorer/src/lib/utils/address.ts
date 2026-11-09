export function inferAddressType(address?: string | null): string | undefined {
  if (!address) return undefined;
  const trimmed = address.trim();
  if (!trimmed) return undefined;
  const lower = trimmed.toLowerCase();
  const sepIndex = lower.indexOf('1');

  // Bech32 / Bech32m (SegWit/Taproot)
  if (sepIndex > 0) {
    const hrp = lower.slice(0, sepIndex);
    if (['bc', 'tb', 'bcrt', 'ltc', 'sb'].includes(hrp)) {
      const dataPart = lower.slice(sepIndex + 1);
      if (!dataPart) return undefined;
      const firstChar = dataPart.charAt(0);
      if (firstChar === 'p') {
        return 'p2tr';
      }
      if (firstChar === 'q') {
        if (dataPart.length === 39 || dataPart.length === 40 || address.length === 42) {
          return 'p2wpkh';
        }
        if (dataPart.length === 59 || dataPart.length === 60 || address.length === 62) {
          return 'p2wsh';
        }
        return 'segwit';
      }
      return 'segwit';
    }
  }

  const base58 =
    /^[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]+$/u.test(trimmed) &&
    trimmed.length >= 26 &&
    trimmed.length <= 35;

  const firstChar = lower[0];
  switch (firstChar) {
    case '1':
    case 'm':
    case 'n': {
      if (!base58) return undefined;
      return 'p2pkh';
    }
    case '3':
    case '2': {
      if (!base58) return undefined;
      return 'p2sh';
    }
    default: {
      return undefined;
    }
  }
}
