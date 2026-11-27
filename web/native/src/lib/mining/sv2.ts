// Stratum v2 mining protocol (standard channels), as binary websocket frames.
// Values are little endian; a u256 is its 32 bytes as on the wire.

const CHANNEL_BIT = 0x8000;
const HEADING = 6;

export const MESSAGE = {
  setupConnection: 0x00,
  setupConnectionSuccess: 0x01,
  setupConnectionError: 0x02,
  openStandardMiningChannel: 0x10,
  openStandardMiningChannelSuccess: 0x11,
  openMiningChannelError: 0x12,
  newMiningJob: 0x15,
  closeChannel: 0x18,
  submitSharesStandard: 0x1a,
  submitSharesSuccess: 0x1c,
  submitSharesError: 0x1d,
  setNewPrevHash: 0x20,
  setTarget: 0x21,
} as const;

// The mining protocol, its version, and the standard jobs flag.
export const MINING_PROTOCOL = 0;
export const PROTOCOL_VERSION = 2;
export const REQUIRES_STANDARD_JOBS = 0x01;

export type ServerMessage =
  | { type: 'setup_success'; version: number; flags: number }
  | { type: 'setup_error'; flags: number; code: string }
  | { type: 'open_success'; request: number; channel: number; target: bigint; prefix: Uint8Array }
  | { type: 'open_error'; request: number; code: string }
  | { type: 'job'; channel: number; job: number; ntime?: number; version: number; merkle: Uint8Array }
  | { type: 'prev_hash'; channel: number; job: number; previous: Uint8Array; ntime: number; bits: number }
  | { type: 'target'; channel: number; target: bigint }
  | { type: 'shares_success'; channel: number; last: number; count: number }
  | { type: 'shares_error'; channel: number; sequence: number; code: string }
  | { type: 'close'; channel: number; code: string }
  | { type: 'unknown'; message: number };

class Writer {
  private bytes: number[] = [];
  u8(value: number) {
    this.bytes.push(value & 0xff);
    return this;
  }
  u16(value: number) {
    return this.u8(value).u8(value >>> 8);
  }
  u32(value: number) {
    return this.u16(value & 0xffff).u16(value >>> 16);
  }
  f32(value: number) {
    const view = new DataView(new ArrayBuffer(4));
    view.setFloat32(0, value, true);
    return this.u32(view.getUint32(0, true));
  }
  str(value: string) {
    const encoded = new TextEncoder().encode(value).slice(0, 255);
    this.u8(encoded.length);
    for (const byte of encoded) this.u8(byte);
    return this;
  }
  raw(value: Uint8Array) {
    for (const byte of value) this.u8(byte);
    return this;
  }
  done(): Uint8Array {
    return Uint8Array.from(this.bytes);
  }
}

class Reader {
  private offset = 0;
  constructor(private readonly view: DataView) {}
  u8() {
    return this.view.getUint8(this.offset++);
  }
  u16() {
    const value = this.view.getUint16(this.offset, true);
    this.offset += 2;
    return value;
  }
  u32() {
    const value = this.view.getUint32(this.offset, true);
    this.offset += 4;
    return value;
  }
  u64() {
    const value = this.view.getBigUint64(this.offset, true);
    this.offset += 8;
    return value;
  }
  bytes(size: number) {
    const value = new Uint8Array(this.view.buffer, this.view.byteOffset + this.offset, size).slice();
    this.offset += size;
    return value;
  }
  str() {
    return new TextDecoder().decode(this.bytes(this.u8()));
  }
}

// The frame of a message: extension type (with the channel bit), message type,
// payload length (u24), payload.
export function frame(message: number, channel: boolean, payload: Uint8Array): Uint8Array {
  return new Writer()
    .u16(channel ? CHANNEL_BIT : 0)
    .u8(message)
    .u8(payload.length)
    .u16(payload.length >>> 8)
    .raw(payload)
    .done();
}

export function setupConnection(host: string, port: number, vendor: string): Uint8Array {
  const payload = new Writer()
    .u8(MINING_PROTOCOL)
    .u16(PROTOCOL_VERSION)
    .u16(PROTOCOL_VERSION)
    .u32(REQUIRES_STANDARD_JOBS)
    .str(host)
    .u16(port)
    .str(vendor)
    .str('')
    .str('')
    .str('')
    .done();
  return frame(MESSAGE.setupConnection, false, payload);
}

export function openStandardMiningChannel(
  request: number,
  identity: string,
  hashrate: number,
  maximumTarget: bigint
): Uint8Array {
  const payload = new Writer().u32(request).str(identity).f32(hashrate).raw(toU256(maximumTarget)).done();
  return frame(MESSAGE.openStandardMiningChannel, false, payload);
}

export function submitSharesStandard(
  channel: number,
  sequence: number,
  job: number,
  nonce: number,
  ntime: number,
  version: number
): Uint8Array {
  const payload = new Writer().u32(channel).u32(sequence).u32(job).u32(nonce).u32(ntime).u32(version).done();
  return frame(MESSAGE.submitSharesStandard, true, payload);
}

// The server message of one frame, undefined if malformed.
export function parseFrame(data: Uint8Array): ServerMessage | undefined {
  if (data.length < HEADING) return undefined;
  const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
  const message = view.getUint8(2);
  const length = view.getUint8(3) | (view.getUint16(4, true) << 8);
  if (data.length !== HEADING + length) return undefined;
  const read = new Reader(new DataView(data.buffer, data.byteOffset + HEADING, length));
  try {
    switch (message) {
      case MESSAGE.setupConnectionSuccess:
        return { type: 'setup_success', version: read.u16(), flags: read.u32() };
      case MESSAGE.setupConnectionError:
        return { type: 'setup_error', flags: read.u32(), code: read.str() };
      case MESSAGE.openStandardMiningChannelSuccess:
        return {
          type: 'open_success',
          request: read.u32(),
          channel: read.u32(),
          target: fromU256(read.bytes(32)),
          prefix: read.bytes(read.u8()),
        };
      case MESSAGE.openMiningChannelError:
        return { type: 'open_error', request: read.u32(), code: read.str() };
      case MESSAGE.newMiningJob: {
        const channel = read.u32();
        const job = read.u32();
        const ntime = read.u8() ? read.u32() : undefined;
        return { type: 'job', channel, job, ntime, version: read.u32(), merkle: read.bytes(32) };
      }
      case MESSAGE.setNewPrevHash:
        return {
          type: 'prev_hash',
          channel: read.u32(),
          job: read.u32(),
          previous: read.bytes(32),
          ntime: read.u32(),
          bits: read.u32(),
        };
      case MESSAGE.setTarget:
        return { type: 'target', channel: read.u32(), target: fromU256(read.bytes(32)) };
      case MESSAGE.submitSharesSuccess:
        return { type: 'shares_success', channel: read.u32(), last: read.u32(), count: read.u32() };
      case MESSAGE.submitSharesError:
        return { type: 'shares_error', channel: read.u32(), sequence: read.u32(), code: read.str() };
      case MESSAGE.closeChannel:
        return { type: 'close', channel: read.u32(), code: read.str() };
      default:
        return { type: 'unknown', message };
    }
  } catch {
    return undefined;
  }
}

// A u256 (little endian) as a number, and back.
export function fromU256(bytes: Uint8Array): bigint {
  let value = 0n;
  for (let i = bytes.length - 1; i >= 0; i--) value = (value << 8n) | BigInt(bytes[i]);
  return value;
}

export function toU256(value: bigint): Uint8Array {
  const out = new Uint8Array(32);
  for (let i = 0; i < 32; i++) out[i] = Number((value >> BigInt(8 * i)) & 0xffn);
  return out;
}

// The 80 byte header of a standard job at the time, with a zero nonce.
export function standardHeader(
  version: number,
  previous: Uint8Array,
  merkle: Uint8Array,
  ntime: number,
  bits: number
): Uint8Array {
  return new Writer().u32(version).raw(previous).raw(merkle).u32(ntime).u32(bits).u32(0).done();
}

// The compact bits as the hex the page displays.
export function bitsHex(bits: number): string {
  return (bits >>> 0).toString(16).padStart(8, '0');
}
