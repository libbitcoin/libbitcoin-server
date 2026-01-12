# Configuration

libbitcoin-server `bs` can be configured in many different ways. Although it can be started without any parameters without issues, it is recommended to adjust the basic parameters to the underlying system and your own requirements.

You can do this by passing a config file to the server at start with the following command:

```sh
$ ./bs --config ./bs.cfg
```
## Config File Structure
The libbitcoin configuration file, referred to below as `bs.cfg`, follows the following structural format:

```
[section]
parameter = value

[section2]
parameter2 = value
```
The configuration includes the following sections:

- **[log]**
  Controls the content and target directories of the log files.
- **[database]**
  Configuration of the store. Primarily specifying the storage location.
- **[bitcoin]**
  Blockchain-specific settings,
- **[node]**
  Configuration of the node. Number of threads, number of parallel block downloads, etc.
- **[network]**
  General network settings, from the user agent string to blacklists.
- **[outbound]**
  Control of P2P network behavior.
- **[inbound]**
  TBD
- **[manual]**
  TBD
- **[web]**
  TBD
- **[explore]**
  TBD
- **[socket]**
  TBD
- **[bitcoind]**
  TBD
- **[electrum]**
  TBD
- **[stratum_v1]**
  TBD
- **[stratum_v2]**
  TBD
- **[forks]**
  TBD

There can be multiple occurences of each section, `bs` will still parse all parameters.

## Sample Configurations
The following sample configurations can be used to start libbitcoin server `bs`. Note that even with these samples, at least the path names and IP addresses must be adjusted.

Use the following config settings to set up the libbitcoin server for milestone sync:

<details>
<summary><b>libbitcoin-server configuration milestone sync</b></summary>

```
[log]
path = /home/user/libbitcoin/log
maximum_size = 10000000

application = true
news = true
session = true
protocol = false
quitting = false
proxy = false
remote = true
fault = true
verbose = false
#objects = false        # debug builds only

[database]
path = /home/user/libbitcoin/blockchain/bitcoin

[bitcoin]
#checkpoint = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0
#milestone  = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0

[node]
threads = 32
thread_priority = true
memory_priority = true

# delay_inbound = true will reject inbound requests until node is current. Speeds up IBD.
delay_inbound = true

# Number of concurrent blocks 
maximum_concurrency = 50000

# Maximum Block Height. Use this for performance testing only.
#maximum_height = 900000

[network]
#path = /home/user/libbitcoin/network
# Thread count should the number of cores of the corresponding system
threads = 16
user_agent = "/libbitcoin4:0.14.2/"

# Blacklisted peers due to protocol violation, version message, incoming, ...
blacklist = 143.20.137.191
blacklist = 143.20.137.34
blacklist = 87.229.79.163
blacklist = 87.229.79.127
blacklist = 87.229.79.102
blacklist = 31.58.215.119
blacklist = 31.58.215.74

[outbound]
# Max number of peer connections
connections = 100

# Max number of addresses in the host pool
host_pool_capacity = 10000

connect_batch_size = 5

[inbound]
# mainnet:  8333, testnet: 18333, regtest: 18444
connections = 0
enable_loopback = false
bind = [::]:8333
bind = 0.0.0.0:8333
```

</details>
<br>

Use the following config settings to setup libbitcoin server for full validation sync:

<details>
<summary><b>libbitcoin-server configuration full validation sync</b></summary>

```
[log]
path = /home/user/libbitcoin/log
maximum_size = 10000000

application = true
news = true
session = true
protocol = false
quitting = false
proxy = false
remote = true
fault = true
verbose = false

[database]
path = /home/user/libbitcoin/blockchain/bitcoin

[bitcoin]
checkpoint = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0
milestone  = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0

[node]
threads = 32
thread_priority = true
memory_priority = true

delay_inbound = true

maximum_concurrency = 50000

[network]
threads = 16
user_agent = "/libbitcoin4:0.14.2/"

blacklist = 143.20.137.191
blacklist = 143.20.137.34
blacklist = 87.229.79.163
blacklist = 87.229.79.127
blacklist = 87.229.79.102
blacklist = 31.58.215.119
blacklist = 31.58.215.74

[outbound]
connections = 100

host_pool_capacity = 10000

connect_batch_size = 5

[inbound]
connections = 0
enable_loopback = false
bind = [::]:8333
bind = 0.0.0.0:8333
```
</details>
<br>

Add the following settings to one of the configs above in order to add client-server endpoints: **(Work in progress/experimental)**

<details>
<summary><b>libbitcoin-server endpoint configuration</b></summary>

```
[manual]
# TBD

[web]
# Web User Interface (http/s, stateless html)
# Ports: 8080/8443 (non-default, prioritizes explore)

path = /home/user/libbitcoin/server/www
default = ws.html
server = libbitcoin/web
connections = 100
bind = 0.0.0.0:8080
bind = [::]:8080

[explore]
# Native RESTful Block Explorer (http/s, stateless html/json)
# Ports: 80/443 (use default)

bind = 0.0.0.0:8181
bind = [::]:8181
connections = 100
path = /home/user/libbitcoin/server/www
default = index.html

[socket]
# Native WebSocket Query Interface (http/s->tcp/s, json, upgrade handshake)
bind = 192.168.0.126:82
connections = 0

[bitcoind]
# bitcoind compatibility interface (http/s, stateless json-rpc-v2)
# TLS not integrated, no default ports.
connections = 0

[electrum]
# electrum compatibility interface (tcp/s, json-rpc-v2)
# clear:50001
# TLS  :50002

bind = 192.168.0.126:8480
connections = 0

[stratum_v1]
# stratum v1 compatibility interface (tcp/s, json-rpc-v1, auth handshake)
# common default: 3333
bind = 192.168.0.126:8580
connections = 0

[stratum_v2]
# stratum v2 compatibility interface (tcp[/s], binary, auth/privacy handshake)
# common default: 3336
bind = 192.168.0.126:8690
connections = 0
```
</details>