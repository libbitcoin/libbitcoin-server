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

    # Esplora REST API options
    parser.addoption(
        "--esplora-host",
        action="store",
        default="localhost",
        help="Host for esplora REST API (default: localhost)"
    )
    parser.addoption(
        "--esplora-port",
        action="store",
        default="3000",
        help="Port for esplora REST API (default: 3000)"
    )

    # Admin web options
    parser.addoption(
        "--admin-host",
        action="store",
        default="localhost",
        help="Host for admin web pages (default: localhost)"
    )
    parser.addoption(
        "--admin-port",
        action="store",
        default="8080",
        help="Port for admin web pages (default: 8080)"
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

    # btcd options
    parser.addoption(
        "--btcd-host",
        action="store",
        default="localhost",
        help="Host for btcd JSON-RPC/websocket (default: localhost)"
    )
    parser.addoption(
        "--btcd-port",
        action="store",
        default="8334",
        help="Port for btcd JSON-RPC/websocket (default: 8334)"
    )
    parser.addoption(
        "--btcd-username",
        action="store",
        default=None,
        help="Username for btcd basic auth / ws authenticate (default: none configured)"
    )
    parser.addoption(
        "--btcd-password",
        action="store",
        default=None,
        help="Password for btcd basic auth / ws authenticate (default: none configured)"
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
    parser.addoption(
        "--electrum-protocol",
        action="store",
        default="1.4:1.6",
        help=(
            "Electrum protocol version range offered in the server.version handshake. "
            "Use 'min:max' (e.g. '1.4:1.6') or a single version (e.g. '1.6'). "
            "The server will negotiate the highest mutually supported version. "
            "(default: 1.4:1.6)"
        )
    )

    # Sparrow options (an independent service on its own binding, serving
    # the electrum interface plus the sparrow methods).
    parser.addoption(
        "--sparrow-host",
        action="store",
        default="localhost",
        help="Host for the sparrow interface (default: localhost)"
    )
    parser.addoption(
        "--sparrow-port",
        action="store",
        default="50003",
        help="Port for the sparrow interface (default: 50003)"
    )

    parser.addoption(
        "--scan-timeout",
        action="store",
        default="1800",
        help="Timeout in seconds for whole-set scans, e.g. gettxoutsetinfo "
             "(default: 1800)"
    )
    parser.addoption(
        "--subscription-timeout",
        action="store",
        default="60",
        help="Timeout in seconds for subscription/notification tests (default: 60)"
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
        default="8880",
        help="Port for Stratum v1 (default: 8880)"
    )
    parser.addoption(
        "--stratum-v1-worker",
        action="store",
        default="worker",
        help="Stratum v1 worker, an address unless the server configures payout_address (default: worker)"
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

    # Robustness options (test_robustness_*.py)
    parser.addoption(
        "--robust-services",
        action="store",
        default="native,electrum,btcd",
        help="Services the robustness suites attack and check after every "
             "test, any of native,esplora,admin,electrum,sparrow,bitcoind,"
             "btcd (default: native,electrum,btcd)"
    )
    parser.addoption(
        "--bitcoind-username",
        action="store",
        default=None,
        help="Username for bitcoind basic auth (default: none configured)"
    )
    parser.addoption(
        "--bitcoind-password",
        action="store",
        default=None,
        help="Password for bitcoind basic auth (default: none configured)"
    )
    parser.addoption(
        "--robust-connections",
        action="store",
        default="32",
        help="The [service] connections limit configured on the server "
             "(default: 32)"
    )
    parser.addoption(
        "--bs-pid",
        action="store",
        default=None,
        help="Server process id, when it runs on this host: enables the "
             "alive and pegged-thread checks after every robustness test"
    )
    parser.addoption(
        "--run-shutdown",
        action="store_true",
        default=False,
        help="Run test_longrun's shutdown test: stops the server process"
    )
    parser.addoption(
        "--no-spin-check",
        action="store_true",
        default=False,
        help="Skip the pegged-thread check (use while the node is syncing)"
    )

    # General options
    parser.addoption(
        "--timeout",
        action="store",
        default="30",
        help="Default timeout for requests in seconds (default: 30)"
    )
    parser.addoption(
        "--log",
        action="store",
        default=None,
        help="Node log (e.g. run.stdout), checked for faults since startup"
    )
    parser.addoption(
        "--run-slow",
        action="store_true",
        default=False,
        help="Run tests marked @pytest.mark.slow (wait on a real external "
             "event, e.g. a live block, up to --subscription-timeout)"
    )


def pytest_configure(config):
    config.addinivalue_line(
        "markers",
        "slow: waits on a real external event (e.g. a live block); "
        "skipped by default, use --run-slow to include"
    )
    config.addinivalue_line(
        "markers",
        "requires_height(height): skipped unless the node has confirmed the "
        "height (the reference data is mainnet)"
    )
    config.addinivalue_line(
        "markers",
        "requires_current: skipped unless the node is current (the pool and "
        "address activity of the reference data are recent)"
    )
    config.addinivalue_line(
        "markers",
        "requires_pool: skipped unless the node's pool is open (relay enabled)"
    )


def pytest_collection_modifyitems(config, items):
    if config.getoption("--run-slow"):
        return
    skip_slow = pytest.mark.skip(reason="needs --run-slow (waits on a real external event)")
    for item in items:
        if "slow" in item.keywords:
            item.add_marker(skip_slow)


@pytest.fixture(autouse=True)
def _requires_height(request):
    marker = request.node.get_closest_marker("requires_height")
    if marker is None:
        return

    import requests
    host = request.config.getoption("--native-host")
    port = request.config.getoption("--native-port")
    try:
        top = requests.get(f"http://{host}:{port}/v1/top?format=json",
            timeout=10).json()["confirmed"]["height"]
    except Exception:
        return

    height = marker.args[0]
    if top < height:
        pytest.skip(f"needs confirmed height {height}, node has {top}")


@pytest.fixture(autouse=True)
def _requires_pool(request):
    if request.node.get_closest_marker("requires_pool") is None:
        return

    import requests
    host = request.config.getoption("--native-host")
    port = request.config.getoption("--native-port")
    try:
        status = requests.get(f"http://{host}:{port}/v1/fees?format=json",
            timeout=10).status_code
    except Exception:
        status = None

    if status != 200:
        pytest.skip("needs an open pool")


@pytest.fixture(autouse=True)
def _requires_current(request):
    if request.node.get_closest_marker("requires_current") is None:
        return

    import requests
    host = request.config.getoption("--bitcoind-rpc-host")
    port = request.config.getoption("--bitcoind-rpc-port")
    payload = {"jsonrpc": "1.0", "id": 1, "method": "getblockchaininfo",
        "params": []}
    try:
        info = requests.post(f"http://{host}:{port}/", json=payload,
            timeout=10).json()["result"]
    except Exception:
        return

    if info.get("initialblockdownload"):
        pytest.skip("needs a current node")


def pytest_report_header(config):
    host = config.getoption("--electrum-host")
    port = config.getoption("--electrum-port")
    proto = config.getoption("--electrum-protocol")
    return [f"electrum: {host}:{port}  protocol offered: {proto}"]


@pytest.fixture(scope="session")
def native_config(request):
    """Configuration for native/REST API tests."""
    return {
        "host": request.config.getoption("--native-host"),
        "port": int(request.config.getoption("--native-port")),
        "base_url": f"http://{request.config.getoption('--native-host')}:{request.config.getoption('--native-port')}/v1"
    }


@pytest.fixture(scope="session")
def admin_config(request):
    """Configuration for admin web page tests."""
    host = request.config.getoption("--admin-host")
    port = int(request.config.getoption("--admin-port"))
    return {
        "host": host,
        "port": port,
        "base_url": f"http://{host}:{port}"
    }


@pytest.fixture(scope="session")
def node_log(request):
    """The node log since its last startup, skipping without --log."""
    path = request.config.getoption("--log")
    if path is None:
        pytest.skip("needs --log")

    with open(path, errors="replace") as file:
        text = file.read()

    start = text.rfind("== startup ==")
    return text[start:] if start >= 0 else text


@pytest.fixture(scope="session")
def esplora_config(request):
    """Configuration for esplora REST API tests."""
    host = request.config.getoption("--esplora-host")
    port = int(request.config.getoption("--esplora-port"))
    return {
        "host": host,
        "port": port,
        "base_url": f"http://{host}:{port}"
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
        "timeout": float(request.config.getoption("--timeout")),
        "scan_timeout": float(request.config.getoption("--scan-timeout"))
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
def bitcoind_rest_config(request, bitcoind_rpc_config):
    """Configuration for bitcoind REST tests (credentials as for RPC)."""
    host = request.config.getoption("--bitcoind-rest-host")
    port = int(request.config.getoption("--bitcoind-rest-port"))

    return {
        "host": host,
        "port": port,
        "base_url": f"http://{host}:{port}/rest",
        "auth": bitcoind_rpc_config["auth"]
    }


@pytest.fixture(scope="session")
def btcd_config(request):
    """Configuration for btcd JSON-RPC/websocket tests."""
    return {
        "host": request.config.getoption("--btcd-host"),
        "port": int(request.config.getoption("--btcd-port")),
        "username": request.config.getoption("--btcd-username"),
        "password": request.config.getoption("--btcd-password"),
        "timeout": float(request.config.getoption("--timeout")),
        "subscription_timeout": float(request.config.getoption("--subscription-timeout")),
    }


@pytest.fixture(scope="session")
def sparrow_config(request):
    """Configuration for sparrow interface tests."""
    proto_str = request.config.getoption("--electrum-protocol")
    if ":" in proto_str:
        proto_min, proto_max = proto_str.split(":", 1)
    else:
        proto_min = proto_max = proto_str.strip()

    return {
        "host": request.config.getoption("--sparrow-host"),
        "port": int(request.config.getoption("--sparrow-port")),
        "timeout": float(request.config.getoption("--timeout")),
        "protocol": [proto_min.strip(), proto_max.strip()],
    }


@pytest.fixture(scope="session")
def robust_env(request):
    """Targets and server-side probes for the robustness suites."""
    from robustness import Environment, Process, Service

    option = request.config.getoption
    proto = option("--electrum-protocol")
    proto_min, _, proto_max = proto.partition(":")
    known = {
        "native": Service("native", option("--native-host"),
                          int(option("--native-port"))),
        "electrum": Service("electrum", option("--electrum-host"),
                            int(option("--electrum-port")),
                            protocol=[proto_min.strip(),
                                      (proto_max or proto_min).strip()]),
        "btcd": Service("btcd", option("--btcd-host"),
                        int(option("--btcd-port")),
                        option("--btcd-username"),
                        option("--btcd-password")),
        "esplora": Service("esplora", option("--esplora-host"),
                           int(option("--esplora-port"))),
        "admin": Service("admin", option("--admin-host"),
                         int(option("--admin-port"))),
        "sparrow": Service("sparrow", option("--sparrow-host"),
                           int(option("--sparrow-port")),
                           protocol=[proto_min.strip(),
                                     (proto_max or proto_min).strip()]),
        "bitcoind": Service("bitcoind", option("--bitcoind-rpc-host"),
                            int(option("--bitcoind-rpc-port")),
                            option("--bitcoind-username"),
                            option("--bitcoind-password")),
    }
    names = [n.strip() for n in option("--robust-services").split(",")
             if n.strip()]
    services = [known[name] for name in names]
    pid = option("--bs-pid")
    return Environment(
        services=services,
        local=all(s.host in ("localhost", "127.0.0.1", "::1")
                  for s in services),
        process=Process(int(pid)) if pid else None,
        spin_check=not option("--no-spin-check"),
        connections=int(option("--robust-connections")),
    )


@pytest.fixture(scope="session")
def electrum_config(request):
    """Configuration for Electrum protocol tests."""
    proto_str = request.config.getoption("--electrum-protocol")
    if ":" in proto_str:
        proto_min, proto_max = proto_str.split(":", 1)
    else:
        proto_min = proto_max = proto_str.strip()

    return {
        "host": request.config.getoption("--electrum-host"),
        "port": int(request.config.getoption("--electrum-port")),
        "timeout": float(request.config.getoption("--timeout")),
        "protocol": [proto_min.strip(), proto_max.strip()],
        "subscription_timeout": float(request.config.getoption("--subscription-timeout")),
    }


@pytest.fixture(scope="session")
def stratum_v1_config(request):
    """Configuration for Stratum v1 tests."""
    return {
        "host": request.config.getoption("--stratum-v1-host"),
        "port": int(request.config.getoption("--stratum-v1-port")),
        "worker": request.config.getoption("--stratum-v1-worker"),
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
