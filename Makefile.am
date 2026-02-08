###############################################################################
#  Copyright (c) 2014-2025 libbitcoin-server developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Automake settings.
#==============================================================================
# Look for macros in the m4 subdirectory.
#------------------------------------------------------------------------------
ACLOCAL_AMFLAGS = -I m4


# Distribute, make and install products.
#==============================================================================
# files => ${pkgconfigdir}
#------------------------------------------------------------------------------
pkgconfig_DATA = \
    libbitcoin-server.pc

# files => ${docdir}
#------------------------------------------------------------------------------
doc_DATA = \
    AUTHORS \
    COPYING \
    ChangeLog \
    INSTALL \
    NEWS \
    README

# src/libbitcoin-server.la => ${libdir}
#------------------------------------------------------------------------------
lib_LTLIBRARIES = src/libbitcoin-server.la
src_libbitcoin_server_la_CPPFLAGS = -I${srcdir}/include -DSYSCONFDIR=\"${sysconfdir}\" ${bitcoin_node_BUILD_CPPFLAGS}
src_libbitcoin_server_la_LIBADD = ${bitcoin_node_LIBS}
src_libbitcoin_server_la_SOURCES = \
    src/configuration.cpp \
    src/error.cpp \
    src/parser.cpp \
    src/server_node.cpp \
    src/settings.cpp \
    src/parsers/bitcoind_query.cpp \
    src/parsers/bitcoind_target.cpp \
    src/parsers/native_query.cpp \
    src/parsers/native_target.cpp \
    src/protocols/protocol_bitcoind_rest.cpp \
    src/protocols/protocol_bitcoind_rpc.cpp \
    src/protocols/protocol_electrum.cpp \
    src/protocols/protocol_electrum_version.cpp \
    src/protocols/protocol_html.cpp \
    src/protocols/protocol_http.cpp \
    src/protocols/protocol_native.cpp \
    src/protocols/protocol_stratum_v1.cpp

# local: test/libbitcoin-server-test
#------------------------------------------------------------------------------
if WITH_TESTS

TESTS = libbitcoin-server-test_runner.sh

check_PROGRAMS = test/libbitcoin-server-test
test_libbitcoin_server_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_node_BUILD_CPPFLAGS}
test_libbitcoin_server_test_LDADD = src/libbitcoin-server.la ${boost_unit_test_framework_LIBS} ${bitcoin_node_LIBS}
test_libbitcoin_server_test_SOURCES = \
    test/configuration.cpp \
    test/error.cpp \
    test/main.cpp \
    test/settings.cpp \
    test/test.cpp \
    test/test.hpp \
    test/parsers/bitcoind_query.cpp \
    test/parsers/bitcoind_target.cpp \
    test/parsers/native_query.cpp \
    test/parsers/native_target.cpp

endif WITH_TESTS

# console/bs => ${bindir}
#------------------------------------------------------------------------------
if WITH_CONSOLE

bin_PROGRAMS = console/bs
console_bs_CPPFLAGS = -I${srcdir}/include ${bitcoin_node_BUILD_CPPFLAGS}
console_bs_LDADD = src/libbitcoin-server.la ${bitcoin_node_LIBS}
console_bs_SOURCES = \
    console/executor.cpp \
    console/executor.hpp \
    console/executor_commands.cpp \
    console/executor_dumps.cpp \
    console/executor_events.cpp \
    console/executor_logging.cpp \
    console/executor_options.cpp \
    console/executor_runner.cpp \
    console/executor_scans.cpp \
    console/executor_signals.cpp \
    console/executor_store.cpp \
    console/executor_test_reader.cpp \
    console/executor_test_writer.cpp \
    console/executor_window.cpp \
    console/localize.hpp \
    console/main.cpp \
    console/stack_trace.cpp \
    console/stack_trace.hpp \
    console/embedded/admin_css.cpp \
    console/embedded/admin_font.cpp \
    console/embedded/admin_html.cpp \
    console/embedded/admin_icon.cpp \
    console/embedded/amin_ecma.cpp \
    console/embedded/embedded.hpp \
    console/embedded/native_css.cpp \
    console/embedded/native_ecma.cpp \
    console/embedded/native_font.cpp \
    console/embedded/native_html.cpp \
    console/embedded/native_icon.cpp

endif WITH_CONSOLE

# files => ${includedir}/bitcoin
#------------------------------------------------------------------------------
include_bitcoindir = ${includedir}/bitcoin
include_bitcoin_HEADERS = \
    include/bitcoin/server.hpp

include_bitcoin_serverdir = ${includedir}/bitcoin/server
include_bitcoin_server_HEADERS = \
    include/bitcoin/server/configuration.hpp \
    include/bitcoin/server/define.hpp \
    include/bitcoin/server/error.hpp \
    include/bitcoin/server/parser.hpp \
    include/bitcoin/server/server_node.hpp \
    include/bitcoin/server/settings.hpp \
    include/bitcoin/server/version.hpp

include_bitcoin_server_channelsdir = ${includedir}/bitcoin/server/channels
include_bitcoin_server_channels_HEADERS = \
    include/bitcoin/server/channels/channel_electrum.hpp \
    include/bitcoin/server/channels/channel_http.hpp \
    include/bitcoin/server/channels/channel_stratum_v1.hpp \
    include/bitcoin/server/channels/channel_stratum_v2.hpp \
    include/bitcoin/server/channels/channel_ws.hpp \
    include/bitcoin/server/channels/channels.hpp

include_bitcoin_server_impl_protocolsdir = ${includedir}/bitcoin/server/impl/protocols
include_bitcoin_server_impl_protocols_HEADERS = \
    include/bitcoin/server/impl/protocols/protocol_native.ipp

include_bitcoin_server_interfacesdir = ${includedir}/bitcoin/server/interfaces
include_bitcoin_server_interfaces_HEADERS = \
    include/bitcoin/server/interfaces/bitcoind_rest.hpp \
    include/bitcoin/server/interfaces/bitcoind_rpc.hpp \
    include/bitcoin/server/interfaces/electrum.hpp \
    include/bitcoin/server/interfaces/interfaces.hpp \
    include/bitcoin/server/interfaces/native.hpp \
    include/bitcoin/server/interfaces/stratum_v1.hpp \
    include/bitcoin/server/interfaces/stratum_v2.hpp \
    include/bitcoin/server/interfaces/types.hpp

include_bitcoin_server_parsersdir = ${includedir}/bitcoin/server/parsers
include_bitcoin_server_parsers_HEADERS = \
    include/bitcoin/server/parsers/bitcoind_query.hpp \
    include/bitcoin/server/parsers/bitcoind_target.hpp \
    include/bitcoin/server/parsers/electrum_version.hpp \
    include/bitcoin/server/parsers/native_query.hpp \
    include/bitcoin/server/parsers/native_target.hpp \
    include/bitcoin/server/parsers/parsers.hpp

include_bitcoin_server_protocolsdir = ${includedir}/bitcoin/server/protocols
include_bitcoin_server_protocols_HEADERS = \
    include/bitcoin/server/protocols/protocol.hpp \
    include/bitcoin/server/protocols/protocol_admin.hpp \
    include/bitcoin/server/protocols/protocol_bitcoind_rest.hpp \
    include/bitcoin/server/protocols/protocol_bitcoind_rpc.hpp \
    include/bitcoin/server/protocols/protocol_electrum.hpp \
    include/bitcoin/server/protocols/protocol_electrum_version.hpp \
    include/bitcoin/server/protocols/protocol_html.hpp \
    include/bitcoin/server/protocols/protocol_http.hpp \
    include/bitcoin/server/protocols/protocol_native.hpp \
    include/bitcoin/server/protocols/protocol_rpc.hpp \
    include/bitcoin/server/protocols/protocol_stratum_v1.hpp \
    include/bitcoin/server/protocols/protocol_stratum_v2.hpp \
    include/bitcoin/server/protocols/protocols.hpp

include_bitcoin_server_sessionsdir = ${includedir}/bitcoin/server/sessions
include_bitcoin_server_sessions_HEADERS = \
    include/bitcoin/server/sessions/session_handshake.hpp \
    include/bitcoin/server/sessions/session_server.hpp \
    include/bitcoin/server/sessions/sessions.hpp

# files => ${bash_completiondir}
#------------------------------------------------------------------------------
if BASH_COMPLETIONDIR

dist_bash_completion_DATA = \
    data/bs

endif BASH_COMPLETIONDIR

# files => ${sysconfdir}/libbitcoin
#------------------------------------------------------------------------------
if WITH_CONSOLE

sysconf_libbitcoindir = ${sysconfdir}/libbitcoin
sysconf_libbitcoin_DATA = \
    data/bs.cfg

endif WITH_CONSOLE


# Custom make targets.
#==============================================================================
# make target: console
#------------------------------------------------------------------------------
target_console = \
    console/bs

console: ${target_console}

