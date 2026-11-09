import { inferAddressType } from './address';

export type SearchClassificationType = 'block-height' | 'hash' | 'address' | 'outpoint' | 'unknown';

export interface SearchClassification {
  type: SearchClassificationType;
  value?: string;
  outputIndex?: number;
}

const HEX_PATTERN_CASE_INSENSITIVE = /^[0-9a-fA-F]+$/;

export function classifySearchQuery(rawInput: string): SearchClassification {
  const query = rawInput.trim();
  if (!query) {
    return { type: 'unknown' };
  }

  const outpoint = /^([0-9a-f]{64}):(\d+)$/iu.exec(query);
  if (outpoint) {
    const outputIndex = Number(outpoint[2]);
    if (Number.isInteger(outputIndex) && outputIndex >= 0 && outputIndex <= 0xffffffff) {
      return { type: 'outpoint', value: outpoint[1].toLowerCase(), outputIndex };
    }
    return { type: 'unknown' };
  }

  if (query.length === 64 && HEX_PATTERN_CASE_INSENSITIVE.test(query)) {
    return { type: 'hash', value: query.toLowerCase() };
  }

  if (/^\d+$/.test(query)) {
    const normalized = query.replace(/^0+/, '') || '0';
    return { type: 'block-height', value: normalized };
  }

  const addressType = inferAddressType(query);
  if (addressType) {
    return { type: 'address', value: query };
  }

  return { type: 'unknown' };
}
