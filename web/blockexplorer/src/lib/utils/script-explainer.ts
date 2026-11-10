import { describeOpcode, isSuccessOpcode, type ScriptContext } from './script-opcodes';

interface ScriptToken {
  offset: number;
  opcode: number;
  name: string;
  description: string;
  data?: string;
}

export interface DecodedScript {
  hex: string;
  tokens: ScriptToken[];
  error?: string;
  notice?: string;
}

export function isHexBytes(value: string) {
  return value.length % 2 === 0 && /^[0-9a-f]*$/iu.test(value);
}

// Analysis limits, not Bitcoin consensus limits. Avoid unbounded parsing/rendering.
const MAX_SCRIPT_BYTES = 100_000;
const MAX_TOKENS = 2_000;

export function decodeScript(value: string, context: ScriptContext = 'legacy'): DecodedScript {
  const hex = value.trim().toLowerCase();
  const tokens: ScriptToken[] = [];
  const result: DecodedScript = { hex, tokens };
  if (hex.length > MAX_SCRIPT_BYTES * 2)
    return {
      ...result,
      error: 'Script exceeds the 100,000-byte explainer limit. Raw data remains available.',
    };
  if (!isHexBytes(hex))
    return {
      ...result,
      error:
        'Script is not valid hexadecimal bytes. Opcode explanations require complete byte pairs.',
    };
  const size = hex.length / 2;
  const byte = (offset: number) => Number.parseInt(hex.slice(offset * 2, offset * 2 + 2), 16);
  let cursor = 0;
  while (cursor < size) {
    if (tokens.length >= MAX_TOKENS)
      return {
        ...result,
        error: 'Stopped at the 2,000-operation explainer limit. Remaining bytes were not decoded.',
      };
    const offset = cursor;
    const opcode = byte(cursor++);
    if (opcode > 0 && opcode <= 0x4e) {
      const lengthBytes = opcode < 0x4c ? 0 : 2 ** (opcode - 0x4c);
      if (cursor + lengthBytes > size)
        return { ...result, error: `Truncated push length at byte ${String(offset)}.` };
      let length = lengthBytes === 0 ? opcode : 0;
      for (let i = 0; i < lengthBytes; i++) length += byte(cursor++) * 256 ** i;
      if (length > size - cursor)
        return {
          ...result,
          error: `Truncated data push at byte ${String(offset)}: declares ${String(length)} bytes, but only ${String(size - cursor)} remain.`,
        };
      tokens.push({
        offset,
        opcode,
        name: lengthBytes ? `OP_PUSHDATA${String(lengthBytes)}` : `PUSH ${String(length)}`,
        description: `Pushes ${String(length)} bytes onto the stack as data, not opcodes.`,
        data: hex.slice(cursor * 2, (cursor + length) * 2),
      });
      cursor += length;
    } else {
      const [name, description] = describeOpcode(opcode, context);
      tokens.push({ offset, opcode, name, description, ...(opcode === 0 ? { data: '' } : {}) });
      if (context === 'tapscript' && isSuccessOpcode(opcode)) {
        return {
          ...result,
          notice:
            'Decoding stops at OP_SUCCESS under BIP 342. Any following bytes are left uninterpreted.',
        };
      }
    }
  }
  return result;
}

export interface ScriptPattern {
  name: string;
  description: string;
  dataLabels?: Partial<Record<number, string>>;
}

const isPublicKey = (hex: string) => /^(?:(?:02|03)[0-9a-f]{64}|04[0-9a-f]{128})$/u.test(hex);

export function identifyScript(
  script: DecodedScript,
  context: 'output' | 'redeem' | 'witness' = 'output'
): ScriptPattern {
  const { hex, tokens } = script;
  if (script.error || script.notice)
    return {
      name: 'Unclassified script',
      description: 'A complete script pattern was not determined.',
    };
  if (hex === '')
    return {
      name: 'Empty script',
      description:
        'Contains no opcodes or data pushes. Spendability depends on the complete spending context.',
    };
  if (/^76a914[0-9a-f]{40}88ac$/u.test(hex))
    return {
      name: 'P2PKH · Pay to public key hash',
      description: 'Requires a public key matching this HASH160 and a valid transaction signature.',
      dataLabels: { 2: 'Public key hash · HASH160' },
    };
  if (context === 'output' && /^a914[0-9a-f]{40}87$/u.test(hex))
    return {
      name: 'P2SH · Pay to script hash',
      description:
        'Commits to a redeem script by HASH160. The spending input supplies that script and its unlocking data.',
      dataLabels: { 1: 'Redeem script hash · HASH160' },
    };
  if (context !== 'witness' && /^0014[0-9a-f]{40}$/u.test(hex))
    return {
      name: 'P2WPKH · SegWit public key hash',
      description:
        'Witness version 0 with a 20-byte public key hash. The witness supplies a signature and public key.',
      dataLabels: { 1: 'Witness program · public key HASH160' },
    };
  if (context !== 'witness' && /^0020[0-9a-f]{64}$/u.test(hex))
    return {
      name: 'P2WSH · SegWit script hash',
      description:
        'Witness version 0 with a 32-byte script hash. The witness supplies arguments followed by the witness script.',
      dataLabels: { 1: 'Witness program · script SHA-256' },
    };
  if (context === 'output' && /^5120[0-9a-f]{64}$/u.test(hex))
    return {
      name: 'P2TR · Taproot',
      description:
        'Witness version 1 with a 32-byte output key. As a native output, supports a key-path signature or a script-path spend. P2SH wrapping does not activate Taproot.',
      dataLabels: { 1: 'Taproot output key · x-only public key' },
    };
  if (tokens[0]?.opcode === 0x6a)
    return {
      name: 'OP_RETURN · Data output',
      description:
        'An output beginning with OP_RETURN is unspendable under current rules. Following pushes can carry application data.',
    };
  if (tokens.length === 2 && isPublicKey(tokens[0].data ?? '') && tokens[1].opcode === 0xac)
    return {
      name: 'P2PK · Pay to public key',
      description: 'Requires a valid transaction signature for the public key in this script.',
      dataLabels: { 0: 'Public key' },
    };
  const required = (tokens[0]?.opcode ?? 0) - 0x50;
  const total = (tokens.at(-2)?.opcode ?? 0) - 0x50;
  if (
    required >= 1 &&
    required <= total &&
    total <= 16 &&
    tokens.length === total + 3 &&
    tokens.at(-1)?.opcode === 0xae &&
    tokens.slice(1, -2).every((token) => isPublicKey(token.data ?? ''))
  ) {
    return {
      name: `${String(required)}-of-${String(total)} multisig`,
      description: `Requires ${String(required)} signatures matching public keys in order, plus the CHECKMULTISIG dummy stack item.`,
    };
  }
  const version = tokens[0]?.opcode === 0 ? 0 : (tokens[0]?.opcode ?? 0) - 0x50;
  if (
    context !== 'witness' &&
    tokens.length === 2 &&
    version >= 0 &&
    version <= 16 &&
    tokens[1].opcode >= 2 &&
    tokens[1].opcode <= 40
  ) {
    return {
      name: `Witness version ${String(version)} program`,
      description:
        'This witness program is outside the standard templates recognized here. No spending behavior is inferred.',
      dataLabels: { 1: 'Witness program' },
    };
  }
  return {
    name: 'Custom script',
    description:
      'Does not match a recognized standard template. Read the opcode breakdown to inspect its structure.',
  };
}

export function asciiPreview(hex: string) {
  if (!hex || !isHexBytes(hex)) return;
  const bytes = hex.slice(0, 240).match(/../gu) ?? [];
  if (
    !bytes.every((byte) => {
      const code = Number.parseInt(byte, 16);
      return code >= 32 && code <= 126;
    })
  )
    return;
  return (
    bytes.map((byte) => String.fromCodePoint(Number.parseInt(byte, 16))).join('') +
    (hex.length > 240 ? '…' : '')
  );
}

interface WitnessItemExplanation {
  index: number;
  label: string;
  description: string;
  scriptContext?: ScriptContext;
}

export interface InputExplanation {
  description: string;
  dataLabels?: Partial<Record<number, string>>;
  redeemScript?: string;
  witnessItems: WitnessItemExplanation[];
}

// Witness stack roles: BIP 141, BIP 341. Roles use the provider's funding-output
// type and item positions; signatures and commitments are not verified here.
export function explainInput(
  script: DecodedScript | undefined,
  witness: string[],
  scriptType?: string,
  isCoinbase = false
): InputExplanation {
  const items: WitnessItemExplanation[] = witness.map((hex, index) => ({
    index,
    label: hex === '' ? 'Empty stack item' : 'Stack item',
    description: isHexBytes(hex)
      ? 'Witness data; its role cannot be determined from bytes alone.'
      : 'This item is not valid hexadecimal bytes.',
  }));
  const result: InputExplanation = {
    description:
      'Unlocking script (scriptSig). It supplies data for the funding output’s spending conditions.',
    witnessItems: items,
  };
  if (isCoinbase) {
    result.description =
      'Coinbase data belongs to the block’s coinbase transaction. It is not an unlocking script and is not executed as Script.';
    if (witness.length === 1 && isHexBytes(witness[0]) && witness[0].length === 64)
      items[0] = {
        index: 0,
        label: 'Witness reserved value',
        description: '32-byte value used in the block’s witness commitment.',
      };
    return result;
  }
  let type = scriptType?.replace(/^v0_/u, '').replace(/^v1_/u, '');
  const aliases: Partial<Record<string, string>> = {
    witness_v0_keyhash: 'p2wpkh',
    witness_v0_scripthash: 'p2wsh',
    witness_v1_taproot: 'p2tr',
  };
  type = type && Object.hasOwn(aliases, type) ? aliases[type] : type;
  if (
    type === 'p2pkh' &&
    script &&
    !script.error &&
    script.tokens.length === 2 &&
    script.tokens[0].data !== undefined &&
    isPublicKey(script.tokens[1].data ?? '')
  ) {
    result.dataLabels = { 0: 'Signature and signature-hash flag', 1: 'Public key' };
  }
  if (
    type === 'p2sh' &&
    script &&
    !script.error &&
    script.tokens.length > 0 &&
    script.tokens.every((token) => token.opcode <= 0x60)
  ) {
    result.redeemScript = script.tokens.at(-1)?.data;
    if (script.tokens.length === 1 && result.redeemScript) {
      if (/^0014[0-9a-f]{40}$/u.test(result.redeemScript)) type = 'p2wpkh';
      if (/^0020[0-9a-f]{64}$/u.test(result.redeemScript)) type = 'p2wsh';
    }
  }
  if (witness.some((hex) => !isHexBytes(hex))) return result;
  if (type === 'p2wpkh' && items.length === 2) {
    items[0] = {
      index: 0,
      label: 'Signature',
      description:
        'ECDSA signature and signature-hash flag for this SegWit v0 input. Signature validity is not checked.',
    };
    items[1] = {
      index: 1,
      label: 'Public key',
      description:
        'Public key whose HASH160 must match the witness program. The hash match is not checked here.',
    };
  } else if (type === 'p2wsh' && items.length > 0) {
    const index = items.length - 1;
    items[index] = {
      index,
      label: 'Witness script',
      description:
        'The last item supplies the script committed to by the funding output’s SHA-256 hash. Earlier items are script arguments.',
      scriptContext: 'legacy',
    };
  } else if (type === 'p2tr' && items.length > 0) {
    let count = items.length;
    if (count >= 2 && witness[count - 1].startsWith('50')) {
      const index = --count;
      items[index] = {
        index,
        label: 'Taproot annex',
        description:
          'Optional final witness item starting with 0x50. Included in signature hashing and removed before choosing the spend path.',
      };
    }
    if (count === 1) {
      items[0] = {
        index: 0,
        label: 'Taproot key-path signature',
        description:
          witness[0].length === 128 || witness[0].length === 130
            ? 'Schnorr signature: 64 bytes, optionally followed by a signature-hash flag. Signature validity is not checked.'
            : 'Key-path position, but the item is not a 64- or 65-byte Schnorr signature.',
      };
    } else {
      const index = count - 1;
      const control = witness[index];
      const bytes = control.length / 2;
      if (bytes >= 33 && bytes <= 4129 && (bytes - 33) % 32 === 0) {
        const version = Number.parseInt(control.slice(0, 2), 16) & 0xfe;
        items[index] = {
          index,
          label: 'Taproot control block',
          description: `Leaf version 0x${version.toString(16).padStart(2, '0')}, output-key parity, internal key, and ${String((bytes - 33) / 32)} Merkle path ${bytes === 65 ? 'node' : 'nodes'}. Commitment is not verified.`,
        };
        items[index - 1] = {
          index: index - 1,
          label: version === 0xc0 ? 'Tapscript' : 'Unknown Taproot leaf script',
          description:
            version === 0xc0
              ? 'Script-path spending conditions interpreted using BIP 342.'
              : 'This leaf version is not interpreted by the explainer.',
          ...(version === 0xc0 ? { scriptContext: 'tapscript' as const } : {}),
        };
      } else {
        items[index] = {
          index,
          label: 'Unrecognized Taproot control block',
          description:
            'The final script-path item does not have the expected control-block length.',
        };
      }
    }
  }
  return result;
}
