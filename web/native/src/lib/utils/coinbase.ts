// The data of a script mnemonic token. A data op is its hex data in brackets,
// with a size prefix where the push is not minimal, as [0011] or [1.0011].
// Bytes that do not parse as an op end the script in parentheses, as (0011).
const DATA = /^(?:\[(?:\d+\.)?([\da-f]*)\]|\(([\da-f]*)\))$/iu;

function ascii(hex: string): string {
  let text = '';
  for (let index = 0; index + 1 < hex.length; index += 2) {
    const byte = Number.parseInt(hex.slice(index, index + 2), 16);
    text += byte >= 0x20 && byte <= 0x7e ? String.fromCodePoint(byte) : '*';
  }
  return text;
}

// The text of a coinbase input script mnemonic, each data token as ascii.
// Under BIP34 the script opens with its block height, which is not text.
export function coinbaseText(mnemonic: string, bip34: boolean): string[] {
  return mnemonic
    .split(' ')
    .slice(bip34 ? 1 : 0)
    .flatMap((token) => {
      const match = DATA.exec(token);
      return match ? [ascii(match[1] ?? match[2])] : [];
    });
}
