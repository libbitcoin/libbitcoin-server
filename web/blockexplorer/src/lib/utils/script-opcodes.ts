// Opcode values: Bitcoin Core src/script/script.h. Tapscript changes: BIP 342.
const OPCODES: Partial<Record<number, readonly [string, string]>> = {
  0x61: ['NOP', 'Does nothing.'],
  0x62: ['VER', 'Reserved opcode; fails if executed under legacy rules.'],
  0x63: ['IF', 'Runs the next branch when the top stack item is true. Consumes that item.'],
  0x64: ['NOTIF', 'Runs the next branch when the top stack item is false. Consumes that item.'],
  0x65: ['VERIF', 'Disabled opcode under legacy and SegWit v0 rules.'],
  0x66: ['VERNOTIF', 'Disabled opcode under legacy and SegWit v0 rules.'],
  0x67: ['ELSE', 'Switches to the other branch of the current conditional.'],
  0x68: ['ENDIF', 'Ends the current conditional.'],
  0x69: ['VERIFY', 'Consumes the top item and fails if it is false.'],
  0x6a: [
    'RETURN',
    'Fails if executed. At the start of an output script, marks an unspendable data output.',
  ],
  0x6b: ['TOALTSTACK', 'Moves the top item to the alternate stack.'],
  0x6c: ['FROMALTSTACK', 'Moves the top alternate-stack item back to the main stack.'],
  0x6d: ['2DROP', 'Removes the top two items.'],
  0x6e: ['2DUP', 'Copies the top two items.'],
  0x6f: ['3DUP', 'Copies the top three items.'],
  0x70: ['2OVER', 'Copies the pair below the top pair to the top.'],
  0x71: ['2ROT', 'Moves the fifth and sixth items to the top as a pair.'],
  0x72: ['2SWAP', 'Swaps the top two pairs.'],
  0x73: ['IFDUP', 'Copies the top item if it is true.'],
  0x74: ['DEPTH', 'Pushes the number of items on the stack.'],
  0x75: ['DROP', 'Removes the top item.'],
  0x76: ['DUP', 'Copies the top stack item.'],
  0x77: ['NIP', 'Removes the item immediately below the top item.'],
  0x78: ['OVER', 'Copies the second item to the top.'],
  0x79: ['PICK', 'Consumes a depth and copies the item at that depth to the top.'],
  0x7a: ['ROLL', 'Consumes a depth and moves the item at that depth to the top.'],
  0x7b: ['ROT', 'Moves the third item to the top.'],
  0x7c: ['SWAP', 'Swaps the top two items.'],
  0x7d: ['TUCK', 'Copies the top item below the second item.'],
  0x7e: ['CAT', 'Disabled under legacy and SegWit v0 rules.'],
  0x7f: ['SUBSTR', 'Disabled under legacy and SegWit v0 rules.'],
  0x80: ['LEFT', 'Disabled under legacy and SegWit v0 rules.'],
  0x81: ['RIGHT', 'Disabled under legacy and SegWit v0 rules.'],
  0x82: ['SIZE', 'Pushes the byte length of the top item without removing it.'],
  0x83: ['INVERT', 'Disabled under legacy and SegWit v0 rules.'],
  0x84: ['AND', 'Disabled under legacy and SegWit v0 rules.'],
  0x85: ['OR', 'Disabled under legacy and SegWit v0 rules.'],
  0x86: ['XOR', 'Disabled under legacy and SegWit v0 rules.'],
  0x87: ['EQUAL', 'Compares the top two byte strings and pushes true or false.'],
  0x88: ['EQUALVERIFY', 'Compares and consumes two byte strings; fails if they differ.'],
  0x89: ['RESERVED1', 'Reserved opcode; fails if executed under legacy rules.'],
  0x8a: ['RESERVED2', 'Reserved opcode; fails if executed under legacy rules.'],
  0x8b: ['1ADD', 'Adds one to the top script number.'],
  0x8c: ['1SUB', 'Subtracts one from the top script number.'],
  0x8d: ['2MUL', 'Disabled under legacy and SegWit v0 rules.'],
  0x8e: ['2DIV', 'Disabled under legacy and SegWit v0 rules.'],
  0x8f: ['NEGATE', 'Changes the sign of the top script number.'],
  0x90: ['ABS', 'Takes the absolute value of the top script number.'],
  0x91: ['NOT', 'Pushes one if the script number is zero, otherwise zero.'],
  0x92: ['0NOTEQUAL', 'Pushes zero if the script number is zero, otherwise one.'],
  0x93: ['ADD', 'Adds the top two script numbers.'],
  0x94: ['SUB', 'Subtracts the top script number from the number below it.'],
  0x95: ['MUL', 'Disabled under legacy and SegWit v0 rules.'],
  0x96: ['DIV', 'Disabled under legacy and SegWit v0 rules.'],
  0x97: ['MOD', 'Disabled under legacy and SegWit v0 rules.'],
  0x98: ['LSHIFT', 'Disabled under legacy and SegWit v0 rules.'],
  0x99: ['RSHIFT', 'Disabled under legacy and SegWit v0 rules.'],
  0x9a: ['BOOLAND', 'Pushes true if both script numbers are nonzero.'],
  0x9b: ['BOOLOR', 'Pushes true if either script number is nonzero.'],
  0x9c: ['NUMEQUAL', 'Compares the top two script numbers for equality.'],
  0x9d: ['NUMEQUALVERIFY', 'Consumes two script numbers; fails if they differ.'],
  0x9e: ['NUMNOTEQUAL', 'Pushes true if the two script numbers differ.'],
  0x9f: ['LESSTHAN', 'Tests whether the second script number is less than the top one.'],
  0xa0: ['GREATERTHAN', 'Tests whether the second script number is greater than the top one.'],
  0xa1: ['LESSTHANOREQUAL', 'Tests whether the second script number is at most the top one.'],
  0xa2: ['GREATERTHANOREQUAL', 'Tests whether the second script number is at least the top one.'],
  0xa3: ['MIN', 'Keeps the smaller of the top two script numbers.'],
  0xa4: ['MAX', 'Keeps the larger of the top two script numbers.'],
  0xa5: ['WITHIN', 'Tests a script number against an inclusive lower and exclusive upper bound.'],
  0xa6: ['RIPEMD160', 'Replaces the top item with its RIPEMD-160 hash.'],
  0xa7: ['SHA1', 'Replaces the top item with its SHA-1 hash.'],
  0xa8: ['SHA256', 'Replaces the top item with its SHA-256 hash.'],
  0xa9: ['HASH160', 'Hashes the top item with SHA-256, then RIPEMD-160.'],
  0xaa: ['HASH256', 'Hashes the top item twice with SHA-256.'],
  0xab: ['CODESEPARATOR', 'Changes which script position is used when computing signature hashes.'],
  0xac: [
    'CHECKSIG',
    'Checks a transaction signature against a public key and pushes true or false.',
  ],
  0xad: [
    'CHECKSIGVERIFY',
    'Checks a transaction signature against a public key; fails if the check fails.',
  ],
  0xae: [
    'CHECKMULTISIG',
    'Checks an ordered set of signatures against public keys. Also consumes an extra dummy item.',
  ],
  0xaf: ['CHECKMULTISIGVERIFY', 'Performs CHECKMULTISIG and fails if the signature checks fail.'],
  0xb0: ['NOP1', 'Reserved for upgrades; does nothing under current consensus rules.'],
  0xb1: [
    'CHECKLOCKTIMEVERIFY',
    'Checks the top number against transaction locktime and input sequence; leaves it on the stack.',
  ],
  0xb2: [
    'CHECKSEQUENCEVERIFY',
    'Checks a relative lock constraint against transaction version and input sequence; leaves it on the stack.',
  ],
  0xba: [
    'CHECKSIGADD',
    'Defined only for tapscript; not an executable legacy or SegWit v0 opcode.',
  ],
  0xff: ['INVALIDOPCODE', 'Undefined opcode; fails if executed.'],
};

export type ScriptContext = 'legacy' | 'tapscript';

export function isSuccessOpcode(opcode: number) {
  return (
    opcode === 80 ||
    opcode === 98 ||
    (opcode >= 126 && opcode <= 129) ||
    (opcode >= 131 && opcode <= 134) ||
    (opcode >= 137 && opcode <= 138) ||
    (opcode >= 141 && opcode <= 142) ||
    (opcode >= 149 && opcode <= 153) ||
    (opcode >= 187 && opcode <= 254)
  );
}

export function describeOpcode(opcode: number, context: ScriptContext): readonly [string, string] {
  if (context === 'tapscript' && isSuccessOpcode(opcode)) {
    return [
      `OP_SUCCESS${String(opcode)}`,
      'BIP 342 success opcode. Stops further script decoding under these rules; this is not a full spend-validation result.',
    ];
  }
  if (opcode === 0) return ['OP_0', 'Pushes an empty byte string, representing false or zero.'];
  if (opcode === 0x4f) return ['OP_1NEGATE', 'Pushes the script number −1.'];
  if (opcode === 0x50)
    return ['OP_RESERVED', 'Reserved opcode; fails if executed under legacy rules.'];
  if (opcode >= 0x51 && opcode <= 0x60)
    return [`OP_${String(opcode - 0x50)}`, `Pushes the script number ${String(opcode - 0x50)}.`];
  if (opcode >= 0xb3 && opcode <= 0xb9)
    return [
      `OP_NOP${String(opcode - 0xaf)}`,
      'Reserved for upgrades; does nothing under current consensus rules.',
    ];
  if (context === 'tapscript' && opcode === 0xba)
    return [
      'OP_CHECKSIGADD',
      'Checks a Schnorr signature and adds one to the counter if it succeeds. An empty signature adds zero.',
    ];
  const known = OPCODES[opcode];
  if (context === 'tapscript' && (opcode === 0xae || opcode === 0xaf)) {
    return [`OP_${known?.[0] ?? 'CHECKMULTISIG'}`, 'Disabled in tapscript; fails if executed.'];
  }
  return known
    ? [`OP_${known[0]}`, known[1]]
    : [
        `OP_UNKNOWN_0x${opcode.toString(16).padStart(2, '0')}`,
        'Unrecognized opcode. No execution result is inferred.',
      ];
}
