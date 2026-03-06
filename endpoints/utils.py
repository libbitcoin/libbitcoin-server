"""
Shared utilities for libbitcoin-server endpoint testing.

Provides common functions for hash operations, reference test data,
and helper utilities used across multiple test modules.
"""

import hashlib


def hex_to_sha256(hex_string: str) -> str:
    """
    Computes the SHA-256 hash of a hex string.

    Args:
        hex_string: Hexadecimal string to hash

    Returns:
        SHA-256 hash as hex string (64 characters)

    Raises:
        ValueError: If hex_string is invalid

    Example:
        >>> hex_to_sha256("41045a6e07b7e579fd850014fd2bd27fbfbb0edc0e892217cd1bd5648b1582fc")
        'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'
    """
    # Remove whitespace and newlines
    cleaned_hex = hex_string.replace(" ", "").replace("\n", "").replace("\r", "")

    try:
        # Hex string → bytes
        data_bytes = bytes.fromhex(cleaned_hex)
    except ValueError as e:
        raise ValueError(f"Invalid hex string: {e}")

    # Calculate SHA256
    sha256_hash = hashlib.sha256(data_bytes).hexdigest()

    return sha256_hash


def double_sha256(hex_string: str) -> str:
    """
    Computes double SHA-256 hash (Bitcoin standard).

    Args:
        hex_string: Hexadecimal string to hash

    Returns:
        Double SHA-256 hash as hex string (64 characters)
    """
    cleaned_hex = hex_string.replace(" ", "").replace("\n", "").replace("\r", "")
    data_bytes = bytes.fromhex(cleaned_hex)

    # First SHA-256
    first_hash = hashlib.sha256(data_bytes).digest()
    # Second SHA-256
    second_hash = hashlib.sha256(first_hash).hexdigest()

    return second_hash


def reverse_hex(hex_string: str) -> str:
    """
    Reverses byte order of a hex string (endianness conversion).

    Args:
        hex_string: Hexadecimal string

    Returns:
        Reversed hex string

    Example:
        >>> reverse_hex("0102030405")
        '0504030201'
    """
    cleaned = hex_string.replace(" ", "").replace("\n", "").replace("\r", "")
    bytes_data = bytes.fromhex(cleaned)
    return bytes_data[::-1].hex()


class ReferenceData:
    """Reference blockchain data for testing."""

    # Genesis block (height 0)
    GENESIS_HEIGHT = 0
    GENESIS_HASH = "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"
    GENESIS_TX_HASH = "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"

    # Block 1 coinbase
    BLOCK1_HEIGHT = 1
    BLOCK1_HASH = "00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048"
    BLOCK1_COINBASE_TX_HASH = "0e3e2357e806b6cdb1f70b54c3a3a17b6714ee1f0e68bebb44a74b1efd512098"

    # Known test block (height 9000)
    KNOWN_HEIGHT = 9000
    KNOWN_BLOCK_HASH = "00000000415c7a0f3e54ed29a3165414e5952fce6848b697696fcc840011a5b5"
    KNOWN_TX_HASH = "a5f71a0b4ccda01fa6b491b6f4d320aa3320df81111d1c28dbbabfa47ec92d88"

    # First Bitcoin transaction (Block 170)
    FIRST_TX_BLOCK_HEIGHT = 170
    FIRST_TX_HASH = "f4184fc596403b9d638783cf57adfe4c75c605f6356fbc91338530e9831e9e16"

    # First SegWit block (height 481824, activation 24 Aug 2017)
    SEGWIT_HEIGHT = 481824
    SEGWIT_BLOCK_HASH = "0000000000000000001c8018d9cb3b742ef25114f27563e3fc4a1902167f9893"
    SEGWIT_TX_HASH_P2WPKH = "dfcec48bb8491856c353306ab5febeb7e99e4d783eedf3de98f3ee0812b92bad"
    SEGWIT_TX_HASH_P2WPKH2 = "f91d0a8a78462bc59398f2c5d7a84fcff491c26ba54c4833478b202796c8aafd"
    SEGWIT_TX_HASH_P2WSH = "461e8a4aa0a0e75c06602c505bd7aa06e7116ba5cd98fd6e046e8cbeb00379d6"

    # Example addresses and script hashes
    EXAMPLE_ADDRESS = "12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX"
    EXAMPLE_SCRIPTHASH = "6e8c6c7183154cae7d476738831af03c292349ced8984a302baed3316062a88b"

    # Output script hash for testing
    KNOWN_OUTPUT_SCRIPT = "41045a6e07b7e579fd850014fd2bd27fbfbb0edc0e892217cd1bd5648b1582fc"
    KNOWN_OUTPUT_SCRIPT_HASH = hex_to_sha256(KNOWN_OUTPUT_SCRIPT)

    # Filter types
    FILTER_TYPE_BASIC = "basic"
    FILTER_TYPE_BLOOM = "bloom"
    FILTER_TYPE_COMPACT = "compact"


class TestConfig:
    """Default test configuration values."""

    # Default timeouts (seconds)
    DEFAULT_HTTP_TIMEOUT = 12.0
    DEFAULT_SOCKET_TIMEOUT = 5.0
    DEFAULT_RPC_TIMEOUT = 30.0

    # Default ports
    DEFAULT_NATIVE_PORT = 8181
    DEFAULT_BITCOIND_RPC_PORT = 8332
    DEFAULT_BITCOIND_REST_PORT = 8332
    DEFAULT_ELECTRUM_PORT = 50001
    DEFAULT_STRATUM_V1_PORT = 3333
    DEFAULT_STRATUM_V2_PORT = 3336

    # Response formats
    FORMAT_JSON = "json"
    FORMAT_TEXT = "text"
    FORMAT_DATA = "data"


def validate_hex_hash(hash_string: str, expected_length: int = 64) -> bool:
    """
    Validates that a string is a valid hexadecimal hash.

    Args:
        hash_string: String to validate
        expected_length: Expected length in characters (default: 64 for SHA-256)

    Returns:
        True if valid, False otherwise
    """
    if not isinstance(hash_string, str):
        return False

    if len(hash_string) != expected_length:
        return False

    try:
        int(hash_string, 16)
        return True
    except ValueError:
        return False


def validate_block_height(height: int) -> bool:
    """
    Validates that a block height is reasonable.

    Args:
        height: Block height to validate

    Returns:
        True if valid, False otherwise
    """
    return isinstance(height, int) and 0 <= height < 10_000_000