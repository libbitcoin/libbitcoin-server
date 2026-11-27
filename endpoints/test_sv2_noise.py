"""
Tests of the Stratum v2 Noise reference (sv2_noise.py) against the fixed-key
vectors in sv2_noise_vectors.json, which SRI noise_sv2 reproduces byte for
byte. These run without a server.

Run with:
    pytest test_sv2_noise.py
"""

from __future__ import annotations

import hashlib
import json
import os

import pytest

import sv2_noise

with open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
        "sv2_noise_vectors.json")) as file:
    VECTORS = json.load(file)

INPUTS = VECTORS["inputs"]
HANDSHAKE = VECTORS["handshake"]
FRAMES = VECTORS["transport"]["frames"]


def value(name):
    return bytes.fromhex(INPUTS[name])


def initiator(trace=None):
    return sv2_noise.Initiator(value("initiator_ephemeral_secret"),
        value("initiator_ephemeral_ellswift"), value("authority_xonly"), trace)


def certificate():
    return sv2_noise.sign_certificate(value("authority_secret"),
        value("responder_static_ellswift"), INPUTS["certificate_version"],
        INPUTS["valid_from"], INPUTS["not_valid_after"],
        value("certificate_aux"))


def responder(trace=None):
    return sv2_noise.Responder(value("responder_ephemeral_secret"),
        value("responder_ephemeral_ellswift"), value("responder_static_secret"),
        value("responder_static_ellswift"), certificate(), trace)


def handshake(now=None):
    """Both sides complete the handshake, (initiator, responder) ciphers."""
    left = initiator()
    right = responder()
    message2, send, receive = right.step_1(left.step_0())
    sent, received = left.step_2(message2, INPUTS["now"] if now is None else now)
    return (sent, received), (receive, send)


def pattern(size):
    return bytes(index & 0xff for index in range(size))


def digests(cipher, direction):
    """Encrypt the frames of one direction in order, their sha256 digests."""
    out = []
    for frame in FRAMES:
        if frame["direction"] == direction:
            ciphertext = sv2_noise.encrypt_frame(cipher, frame["extension_type"],
                frame["message_type"], pattern(frame["payload_length"]))
            out.append(hashlib.sha256(ciphertext).hexdigest())
    return out


def expected(direction):
    return [frame["ciphertext_sha256"] for frame in FRAMES
        if frame["direction"] == direction]


def tampered(message, index):
    out = bytearray(message)
    out[index] ^= 1
    return bytes(out)


def test_authority_key__specification_vector__encoding():
    raw = bytes.fromhex("76637000979c1c11af0c300bcd8c7fe48610fce9b9c11e3daee35ae0b08a7455")
    assert sv2_noise.authority_key(raw) == "9bXiEd8boQVhq7WddEcERUL5tyyJVFYdU8th3HfbNXK3Yw6GRXh"


def test_authority_key__vector_authority__vector_encoding():
    assert sv2_noise.authority_key(value("authority_xonly")) == INPUTS["authority_key"]


def test_certificate__vector_inputs__signature_noise_message():
    assert certificate().hex() == HANDSHAKE["signature_noise_message"]


def test_initiator__step_0__message1():
    trace = []
    message1 = initiator(trace).step_0()
    assert message1.hex() == HANDSHAKE["message1"]
    assert trace == HANDSHAKE["initiator_trace"][:2]


def test_responder__step_1__message2_and_trace():
    trace = []
    message2, _, _ = responder(trace).step_1(bytes.fromhex(HANDSHAKE["message1"]))
    assert message2.hex() == HANDSHAKE["message2"]
    assert trace == HANDSHAKE["responder_trace"]


def test_initiator__step_2__transport_keys_and_trace():
    trace = []
    left = initiator(trace)
    left.step_0()
    sent, received = left.step_2(bytes.fromhex(HANDSHAKE["message2"]), INPUTS["now"])
    assert trace == HANDSHAKE["initiator_trace"]
    assert left.h.hex() == HANDSHAKE["handshake_hash"]
    assert sent.k.hex() == HANDSHAKE["initiator_to_responder_key"]
    assert received.k.hex() == HANDSHAKE["responder_to_initiator_key"]


def test_transport__initiator_frames__digests():
    (sent, _), _ = handshake()
    assert digests(sent, "initiator_to_responder") == expected("initiator_to_responder")


def test_transport__responder_frames__digests():
    _, (_, send) = handshake()
    assert digests(send, "responder_to_initiator") == expected("responder_to_initiator")


def test_transport__inline_frames__decrypt_to_payload():
    (_, received), (_, send) = handshake()
    first = sv2_noise.encrypt_frame(send, 0, 0x1f, b"")
    second = sv2_noise.encrypt_frame(send, 0x8000, 0x20, pattern(1))
    frame1, rest1 = sv2_noise.decrypt_frame(received, first + second)
    frame2, rest2 = sv2_noise.decrypt_frame(received, rest1)
    assert frame1 == (0, 0x1f, b"")
    assert frame2 == (0x8000, 0x20, pattern(1))
    assert rest2 == b""


@pytest.mark.parametrize("index", [0, 64, 143, 144, 233])
def test_initiator__step_2_tampered_message2__raises(index):
    left = initiator()
    left.step_0()
    with pytest.raises((sv2_noise.DecryptFailed, ValueError)):
        left.step_2(tampered(bytes.fromhex(HANDSHAKE["message2"]), index), INPUTS["now"])


@pytest.mark.parametrize("now", [INPUTS["valid_from"] - 1, INPUTS["not_valid_after"] + 1])
def test_initiator__step_2_outside_window__raises(now):
    with pytest.raises(ValueError):
        handshake(now)


def test_cipher__decrypt_failure__nonce_unchanged():
    (sent, _), (receive, _) = handshake()
    frame = sv2_noise.encrypt_frame(sent, 0, 0x1f, b"")
    with pytest.raises(sv2_noise.DecryptFailed):
        sv2_noise.decrypt_frame(receive, tampered(frame, 0))
    assert receive.n == 0
    assert sv2_noise.decrypt_frame(receive, frame) == ((0, 0x1f, b""), b"")


def test_cipher__maximum_nonce__raises():
    cipher = sv2_noise.CipherState(bytes(32))
    cipher.n = sv2_noise.MAX_NONCE
    with pytest.raises(sv2_noise.NonceExhausted):
        cipher.encrypt_with_ad(b"", b"")
    assert cipher.n == sv2_noise.MAX_NONCE
