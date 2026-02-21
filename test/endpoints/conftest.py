"""
pytest configuration for libbitcoin-server endpoint tests.

Provides command-line options for configuring test targets
and shared fixtures for test sessions.
"""

import pytest


def pytest_addoption(parser):
    """Add custom command-line options for pytest."""

    # Native/REST API options
    parser.addoption(
        "--native-host",
        action="store",
        default="localhost",
        help="Host for native/REST API (default: localhost)"
    )
    parser.addoption(
        "--native-port",
        action="store",
        default="8181",
        help="Port for native/REST API (default: 8181)"
    )

    # bitcoind RPC options
    parser.addoption(
        "--bitcoind-rpc-host",
        action="store",
        default="localhost",
        help="Host for bitcoind RPC (default: localhost)"
    )
    parser.addoption(
        "--bitcoind-rpc-port",
        action="store",
        default="8332",
        help="Port for bitcoind RPC (default: 8332)"
    )
    parser.addoption(
        "--bitcoind-auth",
        action="store_true",
        default=False,
        help="Use authentication for bitcoind RPC (cookie file)"
    )
    parser.addoption(
        "--bitcoind-cookie",
        action="store",
        default="/mnt/core/.cookie",
        help="Path to bitcoind cookie file (default: /mnt/core/.cookie)"
    )

    # bitcoind REST options
    parser.addoption(
        "--bitcoind-rest-host",
        action="store",
        default="localhost",
        help="Host for bitcoind REST (default: localhost)"
    )
    parser.addoption(
        "--bitcoind-rest-port",
        action="store",
        default="8332",
        help="Port for bitcoind REST (default: 8332)"
    )

    # Electrum options
    parser.addoption(
        "--electrum-host",
        action="store",
        default="localhost",
        help="Host for Electrum protocol (default: localhost)"
    )
    parser.addoption(
        "--electrum-port",
        action="store",
        default="50001",
        help="Port for Electrum protocol (default: 50001)"
    )

    # Stratum v1 options
    parser.addoption(
        "--stratum-v1-host",
        action="store",
        default="localhost",
        help="Host for Stratum v1 (default: localhost)"
    )
    parser.addoption(
        "--stratum-v1-port",
        action="store",
        default="3333",
        help="Port for Stratum v1 (default: 3333)"
    )

    # Stratum v2 options
    parser.addoption(
        "--stratum-v2-host",
        action="store",
        default="localhost",
        help="Host for Stratum v2 (default: localhost)"
    )
    parser.addoption(
        "--stratum-v2-port",
        action="store",
        default="3336",
        help="Port for Stratum v2 (default: 3336)"
    )

    # General options
    parser.addoption(
        "--timeout",
        action="store",
        default="30",
        help="Default timeout for requests in seconds (default: 30)"
    )


@pytest.fixture(scope="session")
def native_config(request):
    """Configuration for native/REST API tests."""
    return {
        "host": request.config.getoption("--native-host"),
        "port": int(request.config.getoption("--native-port")),
        "base_url": f"http://{request.config.getoption('--native-host')}:{request.config.getoption('--native-port')}/v1"
    }


@pytest.fixture(scope="session")
def bitcoind_rpc_config(request):
    """Configuration for bitcoind RPC tests."""
    host = request.config.getoption("--bitcoind-rpc-host")
    port = int(request.config.getoption("--bitcoind-rpc-port"))

    config = {
        "host": host,
        "port": port,
        "url": f"http://{host}:{port}/",
        "use_auth": request.config.getoption("--bitcoind-auth"),
        "cookie_path": request.config.getoption("--bitcoind-cookie"),
        "timeout": float(request.config.getoption("--timeout"))
    }

    # Load authentication if requested
    if config["use_auth"]:
        try:
            with open(config["cookie_path"], "r") as f:
                cookie_content = f.read().strip()
            if ":" not in cookie_content:
                raise ValueError("Cookie file invalid, missing ':' separator")
            config["auth"] = tuple(cookie_content.split(":", 1))
        except FileNotFoundError:
            pytest.exit(f"Cookie file not found: {config['cookie_path']}")
        except Exception as e:
            pytest.exit(f"Cannot read cookie file: {e}")
    else:
        config["auth"] = None

    return config


@pytest.fixture(scope="session")
def bitcoind_rest_config(request):
    """Configuration for bitcoind REST tests."""
    host = request.config.getoption("--bitcoind-rest-host")
    port = int(request.config.getoption("--bitcoind-rest-port"))

    return {
        "host": host,
        "port": port,
        "base_url": f"http://{host}:{port}/rest"
    }


@pytest.fixture(scope="session")
def electrum_config(request):
    """Configuration for Electrum protocol tests."""
    return {
        "host": request.config.getoption("--electrum-host"),
        "port": int(request.config.getoption("--electrum-port")),
        "timeout": float(request.config.getoption("--timeout"))
    }


@pytest.fixture(scope="session")
def stratum_v1_config(request):
    """Configuration for Stratum v1 tests."""
    return {
        "host": request.config.getoption("--stratum-v1-host"),
        "port": int(request.config.getoption("--stratum-v1-port")),
        "timeout": float(request.config.getoption("--timeout"))
    }


@pytest.fixture(scope="session")
def stratum_v2_config(request):
    """Configuration for Stratum v2 tests."""
    return {
        "host": request.config.getoption("--stratum-v2-host"),
        "port": int(request.config.getoption("--stratum-v2-port")),
        "timeout": float(request.config.getoption("--timeout"))
    }
