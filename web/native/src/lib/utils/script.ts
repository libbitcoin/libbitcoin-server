// A null data (op_return) script, from its mnemonic.
export function isNullData(mnemonic?: string): boolean {
  return mnemonic?.trim().split(/\s+/u)[0] === 'return';
}
