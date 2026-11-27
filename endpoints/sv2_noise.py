"""Stratum v2 Noise NX (sv2-spec section 4), written from the spec text over
the BIP324 and BIP340 reference implementations."""

import hashlib

import bip324_reference as bip324
import bip340_reference as bip340

PROTOCOL_NAME = b'Noise_NX_Secp256k1+EllSwift_ChaChaPoly_SHA256'
MAC_LEN = 16
MAX_CT_LEN = 65535
MAX_PT_LEN = MAX_CT_LEN - MAC_LEN
HEADER_LEN = 6
MAX_NONCE = 2**64 - 1
CERTIFICATE_VERSION = 0


def sha256(data):
    return hashlib.sha256(data).digest()


def hmac_hash(key, data):
    return bip324.hmac_sha256(key, data)


def hkdf2(chaining_key, material):
    temp = hmac_hash(chaining_key, material)
    out1 = hmac_hash(temp, b'\x01')
    out2 = hmac_hash(temp, out1 + b'\x02')
    return out1, out2


def nonce_bytes(n):
    return b'\x00' * 4 + n.to_bytes(8, 'little')


class NonceExhausted(Exception):
    pass


class DecryptFailed(Exception):
    pass


class CipherState:
    def __init__(self, key=None):
        self.k = key
        self.n = 0

    def initialize_key(self, key):
        self.k = key
        self.n = 0

    def encrypt_with_ad(self, ad, plaintext):
        if self.k is None:
            return plaintext
        if self.n == MAX_NONCE:
            raise NonceExhausted()
        out = bip324.aead_chacha20_poly1305_encrypt(self.k, nonce_bytes(self.n), ad, plaintext)
        self.n += 1
        return out

    def decrypt_with_ad(self, ad, ciphertext):
        if self.k is None:
            return ciphertext
        if self.n == MAX_NONCE:
            raise NonceExhausted()
        out = bip324.aead_chacha20_poly1305_decrypt(self.k, nonce_bytes(self.n), ad, ciphertext)
        if out is None:
            raise DecryptFailed()
        self.n += 1
        return out


def ecdh(secret, theirs, ours, initiator):
    return bip324.v2_ecdh(secret, theirs, ours, initiator)


def xonly(ellswift):
    return bip324.ellswift_decode(ellswift)


def xonly_of_secret(secret):
    return bip340.pubkey_gen(secret)


def ellswift_encode(secret, seed):
    """A deterministic ElligatorSwift encoding of the secret's public key."""
    x = (int.from_bytes(secret, 'big') * bip324.SECP256K1_G).x
    counter = 0
    while True:
        draw = sha256(seed + counter.to_bytes(4, 'little'))
        counter += 1
        u = bip324.FE(int.from_bytes(draw, 'big') % bip324.FE.SIZE)
        if u == 0:
            continue
        t = bip324.xswiftec_inv(x, u, draw[0] & 7)
        if t is not None:
            return u.to_bytes() + t.to_bytes()


def signed_header(version, valid_from, not_valid_after):
    return (version.to_bytes(2, 'little') + valid_from.to_bytes(4, 'little') +
        not_valid_after.to_bytes(4, 'little'))


def certificate_hash(version, valid_from, not_valid_after, server_xonly):
    return sha256(signed_header(version, valid_from, not_valid_after) + server_xonly)


def sign_certificate(authority_secret, server_ellswift, version, valid_from,
        not_valid_after, aux):
    digest = certificate_hash(version, valid_from, not_valid_after, xonly(server_ellswift))
    signature = bip340.schnorr_sign(digest, authority_secret, aux)
    return signed_header(version, valid_from, not_valid_after) + signature


def verify_certificate(message, server_ellswift, authority_xonly, now):
    version = int.from_bytes(message[0:2], 'little')
    valid_from = int.from_bytes(message[2:6], 'little')
    not_valid_after = int.from_bytes(message[6:10], 'little')
    if version != CERTIFICATE_VERSION:
        return False
    if not (valid_from <= now <= not_valid_after):
        return False
    digest = certificate_hash(version, valid_from, not_valid_after, xonly(server_ellswift))
    return bip340.schnorr_verify(digest, authority_xonly, message[10:74])


class Handshake:
    def __init__(self, trace=None):
        self.h = sha256(PROTOCOL_NAME)
        self.ck = self.h
        self.h = sha256(self.h)
        self.cs = CipherState()
        self.trace = trace if trace is not None else []
        self.log('initialized')

    def log(self, step):
        self.trace.append({'step': step, 'h': self.h.hex(), 'ck': self.ck.hex()})

    def mix_hash(self, data):
        self.h = sha256(self.h + data)

    def mix_key(self, material):
        self.ck, temp = hkdf2(self.ck, material)
        self.cs.initialize_key(temp)

    def encrypt_and_hash(self, plaintext):
        ciphertext = self.cs.encrypt_with_ad(self.h, plaintext)
        self.mix_hash(ciphertext)
        return ciphertext

    def decrypt_and_hash(self, ciphertext):
        plaintext = self.cs.decrypt_with_ad(self.h, ciphertext)
        self.mix_hash(ciphertext)
        return plaintext

    def split(self):
        k1, k2 = hkdf2(self.ck, b'')
        return CipherState(k1), CipherState(k2)


class Initiator(Handshake):
    def __init__(self, e_secret, e_ellswift, authority_xonly, trace=None):
        super().__init__(trace)
        self.e = e_secret
        self.e_ellswift = e_ellswift
        self.authority = authority_xonly

    def step_0(self):
        out = self.e_ellswift
        self.mix_hash(self.e_ellswift)
        out += self.encrypt_and_hash(b'')
        self.log('message1')
        return out

    def step_2(self, message, now):
        if len(message) != 234:
            raise ValueError('message2 size')
        re = message[0:64]
        self.mix_hash(re)
        self.mix_key(ecdh(self.e, re, self.e_ellswift, True))
        self.log('ee')
        rs = self.decrypt_and_hash(message[64:144])
        self.mix_key(ecdh(self.e, rs, self.e_ellswift, True))
        self.log('es')
        certificate = self.decrypt_and_hash(message[144:234])
        self.log('message2')
        if not verify_certificate(certificate, rs, self.authority, now):
            raise ValueError('certificate')
        self.server_ellswift = rs
        self.certificate = certificate
        send, receive = self.split()
        return send, receive


class Responder(Handshake):
    def __init__(self, e_secret, e_ellswift, s_secret, s_ellswift, certificate, trace=None):
        super().__init__(trace)
        self.e = e_secret
        self.e_ellswift = e_ellswift
        self.s = s_secret
        self.s_ellswift = s_ellswift
        self.certificate = certificate

    def step_1(self, message):
        if len(message) != 64:
            raise ValueError('message1 size')
        re = message[0:64]
        self.mix_hash(re)
        self.decrypt_and_hash(b'')
        self.log('message1')
        out = self.e_ellswift
        self.mix_hash(self.e_ellswift)
        self.mix_key(ecdh(self.e, re, self.e_ellswift, False))
        self.log('ee')
        out += self.encrypt_and_hash(self.s_ellswift)
        self.mix_key(ecdh(self.s, re, self.s_ellswift, False))
        self.log('es')
        out += self.encrypt_and_hash(self.certificate)
        self.log('message2')
        receive, send = self.split()
        return out, send, receive


def ciphertext_length(plaintext_length):
    remainder = plaintext_length % MAX_PT_LEN
    if remainder > 0:
        remainder += MAC_LEN
    return plaintext_length // MAX_PT_LEN * MAX_CT_LEN + remainder


def encrypt_frame(cs, extension_type, message_type, payload):
    header = (extension_type.to_bytes(2, 'little') + bytes([message_type]) +
        len(payload).to_bytes(3, 'little'))
    out = cs.encrypt_with_ad(b'', header)
    for start in range(0, len(payload), MAX_PT_LEN):
        out += cs.encrypt_with_ad(b'', payload[start:start + MAX_PT_LEN])
    return out


def decrypt_frame(cs, data):
    """Decrypt one frame from the front of data, (header fields, payload, rest)."""
    header = cs.decrypt_with_ad(b'', data[:HEADER_LEN + MAC_LEN])
    extension_type = int.from_bytes(header[0:2], 'little')
    message_type = header[2]
    length = int.from_bytes(header[3:6], 'little')
    body = data[HEADER_LEN + MAC_LEN:HEADER_LEN + MAC_LEN + ciphertext_length(length)]
    payload = b''
    for start in range(0, len(body), MAX_CT_LEN):
        payload += cs.decrypt_with_ad(b'', body[start:start + MAX_CT_LEN])
    rest = data[HEADER_LEN + MAC_LEN + len(body):]
    return (extension_type, message_type, payload), rest


BASE58 = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'


def base58(data):
    value = int.from_bytes(data, 'big')
    out = ''
    while value:
        value, digit = divmod(value, 58)
        out = BASE58[digit] + out
    return '1' * (len(data) - len(data.lstrip(b'\x00'))) + out


def authority_key(authority_xonly):
    payload = b'\x01\x00' + authority_xonly
    return base58(payload + sha256(sha256(payload))[:4])
