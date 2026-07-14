###############################################################################
#  Copyright (c) 2014-2026 libbitcoin-server developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Automake settings.
#==============================================================================
# Look for macros in the 'm4' subdirectory.
#------------------------------------------------------------------------------
ACLOCAL_AMFLAGS = -I m4

# Distribute data.
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

# Libraries.
#==============================================================================
# Target library 'src/libbitcoin-server.la'
#------------------------------------------------------------------------------
lib_LTLIBRARIES = src/libbitcoin-server.la

src_libbitcoin_server_la_LIBS = src/libbitcoin-server.la

src_libbitcoin_server_la_CPPFLAGS = \
    -I${srcdir}/../../include \
    ${libbitcoin_node_BUILD_CPPFLAGS}

src_libbitcoin_server_la_LDFLAGS = \
    ${libbitcoin_node_LDFLAGS}

src_libbitcoin_server_la_LIBADD = \
    ${libbitcoin_node_LIBS}

src_libbitcoin_server_la_SOURCES = \
    ${srcdir}/../../src/configuration.cpp \
    ${srcdir}/../../src/error.cpp \
    ${srcdir}/../../src/parser.cpp \
    ${srcdir}/../../src/server_node.cpp \
    ${srcdir}/../../src/settings.cpp \
    ${srcdir}/../../src/parsers/admin_query.cpp \
    ${srcdir}/../../src/parsers/admin_target.cpp \
    ${srcdir}/../../src/parsers/bitcoind_target.cpp \
    ${srcdir}/../../src/parsers/electrum_version.cpp \
    ${srcdir}/../../src/parsers/native_query.cpp \
    ${srcdir}/../../src/parsers/native_target.cpp \
    ${srcdir}/../../src/protocols/protocol_html.cpp \
    ${srcdir}/../../src/protocols/protocol_http.cpp \
    ${srcdir}/../../src/protocols/admin/protocol_admin.cpp \
    ${srcdir}/../../src/protocols/bitcoind/protocol_bitcoind_rest.cpp \
    ${srcdir}/../../src/protocols/bitcoind/protocol_bitcoind_rpc.cpp \
    ${srcdir}/../../src/protocols/bitcoind/protocol_bitcoind_rpc_json.cpp \
    ${srcdir}/../../src/protocols/electrum/protocol_electrum.cpp \
    ${srcdir}/../../src/protocols/electrum/protocol_electrum_addresses.cpp \
    ${srcdir}/../../src/protocols/electrum/protocol_electrum_fees.cpp \
    ${srcdir}/../../src/protocols/electrum/protocol_electrum_headers.cpp \
    ${srcdir}/../../src/protocols/electrum/protocol_electrum_mempool.cpp \
    ${srcdir}/../../src/protocols/electrum/protocol_electrum_outpoints.cpp \
    ${srcdir}/../../src/protocols/electrum/protocol_electrum_scripthash.cpp \
    ${srcdir}/../../src/protocols/electrum/protocol_electrum_scriptpubkey.cpp \
    ${srcdir}/../../src/protocols/electrum/protocol_electrum_server.cpp \
    ${srcdir}/../../src/protocols/electrum/protocol_electrum_subscribe.cpp \
    ${srcdir}/../../src/protocols/electrum/protocol_electrum_transactions.cpp \
    ${srcdir}/../../src/protocols/electrum/protocol_electrum_version.cpp \
    ${srcdir}/../../src/protocols/native/protocol_native.cpp \
    ${srcdir}/../../src/protocols/native/protocol_native_address.cpp \
    ${srcdir}/../../src/protocols/native/protocol_native_block.cpp \
    ${srcdir}/../../src/protocols/native/protocol_native_configuration.cpp \
    ${srcdir}/../../src/protocols/native/protocol_native_input.cpp \
    ${srcdir}/../../src/protocols/native/protocol_native_output.cpp \
    ${srcdir}/../../src/protocols/native/protocol_native_tx.cpp \
    ${srcdir}/../../src/protocols/stratum_v1/protocol_stratum_v1.cpp

include_bitcoindir = \
    ${includedir}/bitcoin

include_bitcoin_HEADERS = \
    ${srcdir}/../../include/bitcoin/server.hpp

include_bitcoin_serverdir = \
    ${includedir}/bitcoin/server

include_bitcoin_server_HEADERS = \
    ${srcdir}/../../include/bitcoin/server/configuration.hpp \
    ${srcdir}/../../include/bitcoin/server/define.hpp \
    ${srcdir}/../../include/bitcoin/server/error.hpp \
    ${srcdir}/../../include/bitcoin/server/parser.hpp \
    ${srcdir}/../../include/bitcoin/server/server_node.hpp \
    ${srcdir}/../../include/bitcoin/server/settings.hpp \
    ${srcdir}/../../include/bitcoin/server/version.hpp

include_bitcoin_server_channelsdir = \
    ${includedir}/bitcoin/server/channels

include_bitcoin_server_channels_HEADERS = \
    ${srcdir}/../../include/bitcoin/server/channels/channel.hpp \
    ${srcdir}/../../include/bitcoin/server/channels/channel_electrum.hpp \
    ${srcdir}/../../include/bitcoin/server/channels/channel_http.hpp \
    ${srcdir}/../../include/bitcoin/server/channels/channel_stratum_v1.hpp \
    ${srcdir}/../../include/bitcoin/server/channels/channel_stratum_v2.hpp \
    ${srcdir}/../../include/bitcoin/server/channels/channels.hpp

include_bitcoin_server_impl_protocolsdir = \
    ${includedir}/bitcoin/server/impl/protocols

include_bitcoin_server_impl_protocols_HEADERS = \
    ${srcdir}/../../include/bitcoin/server/impl/protocols/protocol_native.ipp

include_bitcoin_server_interfacesdir = \
    ${includedir}/bitcoin/server/interfaces

include_bitcoin_server_interfaces_HEADERS = \
    ${srcdir}/../../include/bitcoin/server/interfaces/admin.hpp \
    ${srcdir}/../../include/bitcoin/server/interfaces/bitcoind_rest.hpp \
    ${srcdir}/../../include/bitcoin/server/interfaces/bitcoind_rpc.hpp \
    ${srcdir}/../../include/bitcoin/server/interfaces/btcd.hpp \
    ${srcdir}/../../include/bitcoin/server/interfaces/electrum.hpp \
    ${srcdir}/../../include/bitcoin/server/interfaces/interfaces.hpp \
    ${srcdir}/../../include/bitcoin/server/interfaces/native.hpp \
    ${srcdir}/../../include/bitcoin/server/interfaces/stratum_v1.hpp \
    ${srcdir}/../../include/bitcoin/server/interfaces/stratum_v2.hpp \
    ${srcdir}/../../include/bitcoin/server/interfaces/types.hpp

include_bitcoin_server_parsersdir = \
    ${includedir}/bitcoin/server/parsers

include_bitcoin_server_parsers_HEADERS = \
    ${srcdir}/../../include/bitcoin/server/parsers/admin_query.hpp \
    ${srcdir}/../../include/bitcoin/server/parsers/admin_target.hpp \
    ${srcdir}/../../include/bitcoin/server/parsers/bitcoind_target.hpp \
    ${srcdir}/../../include/bitcoin/server/parsers/electrum_version.hpp \
    ${srcdir}/../../include/bitcoin/server/parsers/native_query.hpp \
    ${srcdir}/../../include/bitcoin/server/parsers/native_target.hpp \
    ${srcdir}/../../include/bitcoin/server/parsers/parsers.hpp

include_bitcoin_server_protocolsdir = \
    ${includedir}/bitcoin/server/protocols

include_bitcoin_server_protocols_HEADERS = \
    ${srcdir}/../../include/bitcoin/server/protocols/protocol.hpp \
    ${srcdir}/../../include/bitcoin/server/protocols/protocol_admin.hpp \
    ${srcdir}/../../include/bitcoin/server/protocols/protocol_bitcoind_rest.hpp \
    ${srcdir}/../../include/bitcoin/server/protocols/protocol_bitcoind_rpc.hpp \
    ${srcdir}/../../include/bitcoin/server/protocols/protocol_electrum.hpp \
    ${srcdir}/../../include/bitcoin/server/protocols/protocol_electrum_version.hpp \
    ${srcdir}/../../include/bitcoin/server/protocols/protocol_html.hpp \
    ${srcdir}/../../include/bitcoin/server/protocols/protocol_http.hpp \
    ${srcdir}/../../include/bitcoin/server/protocols/protocol_native.hpp \
    ${srcdir}/../../include/bitcoin/server/protocols/protocol_rpc.hpp \
    ${srcdir}/../../include/bitcoin/server/protocols/protocol_stratum_v1.hpp \
    ${srcdir}/../../include/bitcoin/server/protocols/protocol_stratum_v2.hpp \
    ${srcdir}/../../include/bitcoin/server/protocols/protocols.hpp

include_bitcoin_server_sessionsdir = \
    ${includedir}/bitcoin/server/sessions

include_bitcoin_server_sessions_HEADERS = \
    ${srcdir}/../../include/bitcoin/server/sessions/session.hpp \
    ${srcdir}/../../include/bitcoin/server/sessions/session_handshake.hpp \
    ${srcdir}/../../include/bitcoin/server/sessions/session_server.hpp \
    ${srcdir}/../../include/bitcoin/server/sessions/sessions.hpp

# Tests.
#==============================================================================
# Target test 'test/libbitcoin-server-test'
#------------------------------------------------------------------------------
if WITH_TESTS

check_PROGRAMS = test/libbitcoin-server-test

test_libbitcoin_server_test_CPPFLAGS = \
    ${boost_BUILD_CPPFLAGS} \
    ${boost_unit_test_framework_BUILD_CPPFLAGS} \
    ${src_libbitcoin_server_la_CPPFLAGS}

test_libbitcoin_server_test_LDFLAGS = \
    ${boost_LDFLAGS} \
    ${boost_unit_test_framework_LDFLAGS} \
    ${src_libbitcoin_server_la_LDFLAGS}

test_libbitcoin_server_test_LDADD = \
    ${boost_LIBS} \
    ${boost_unit_test_framework_LIBS} \
    ${src_libbitcoin_server_la_LIBS} \
    ${src_libbitcoin_server_la_LIBADD}

test_libbitcoin_server_test_SOURCES = \
    ${srcdir}/../../test/configuration.cpp \
    ${srcdir}/../../test/error.cpp \
    ${srcdir}/../../test/main.cpp \
    ${srcdir}/../../test/settings.cpp \
    ${srcdir}/../../test/test.cpp \
    ${srcdir}/../../test/mocks/blocks.cpp \
    ${srcdir}/../../test/parsers/admin_query.cpp \
    ${srcdir}/../../test/parsers/admin_target.cpp \
    ${srcdir}/../../test/parsers/bitcoind_target.cpp \
    ${srcdir}/../../test/parsers/electrum_version.cpp \
    ${srcdir}/../../test/parsers/native_query.cpp \
    ${srcdir}/../../test/parsers/native_target.cpp \
    ${srcdir}/../../test/protocols/admin/admin_diagnostics.cpp \
    ${srcdir}/../../test/protocols/admin/admin_setup_fixture.cpp \
    ${srcdir}/../../test/protocols/bitcoind/bitcoind_json.cpp \
    ${srcdir}/../../test/protocols/bitcoind/bitcoind_rest.cpp \
    ${srcdir}/../../test/protocols/bitcoind/bitcoind_rpc.cpp \
    ${srcdir}/../../test/protocols/bitcoind/bitcoind_setup_fixture.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_addresses.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_disabled.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_fees.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_headers.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_mempool.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_outpoints.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_scripthash.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_scriptpubkey.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_server.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_setup_fixture.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_subscribe.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_transactions.cpp \
    ${srcdir}/../../test/protocols/electrum/electrum_version.cpp \
    ${srcdir}/../../test/protocols/native/native_address.cpp \
    ${srcdir}/../../test/protocols/native/native_block.cpp \
    ${srcdir}/../../test/protocols/native/native_configuration.cpp \
    ${srcdir}/../../test/protocols/native/native_input.cpp \
    ${srcdir}/../../test/protocols/native/native_output.cpp \
    ${srcdir}/../../test/protocols/native/native_setup_fixture.cpp \
    ${srcdir}/../../test/protocols/native/native_tx.cpp

TESTS = test_runner.sh

endif WITH_TESTS

# Installed Binaries.
#==============================================================================
# Target binary 'console/bs'
#------------------------------------------------------------------------------
if WITH_CONSOLE

bin_PROGRAMS = console/bs

console_bs_CPPFLAGS = \
    -I${srcdir}/../../console \
    ${src_libbitcoin_server_la_CPPFLAGS}

console_bs_LDFLAGS = \
    ${src_libbitcoin_server_la_LDFLAGS}

console_bs_LDADD = \
    ${src_libbitcoin_server_la_LIBS} \
    ${src_libbitcoin_server_la_LIBADD}

console_bs_SOURCES = \
    ${srcdir}/../../console/executor.cpp \
    ${srcdir}/../../console/executor_commands.cpp \
    ${srcdir}/../../console/executor_daemon.cpp \
    ${srcdir}/../../console/executor_dumps.cpp \
    ${srcdir}/../../console/executor_events.cpp \
    ${srcdir}/../../console/executor_logging.cpp \
    ${srcdir}/../../console/executor_options.cpp \
    ${srcdir}/../../console/executor_paging.cpp \
    ${srcdir}/../../console/executor_runner.cpp \
    ${srcdir}/../../console/executor_scans.cpp \
    ${srcdir}/../../console/executor_signals.cpp \
    ${srcdir}/../../console/executor_store.cpp \
    ${srcdir}/../../console/executor_test_reader.cpp \
    ${srcdir}/../../console/executor_test_writer.cpp \
    ${srcdir}/../../console/executor_window.cpp \
    ${srcdir}/../../console/main.cpp \
    ${srcdir}/../../console/stack_trace.cpp \
    ${srcdir}/../../console/embedded/admin_css.cpp \
    ${srcdir}/../../console/embedded/admin_font.cpp \
    ${srcdir}/../../console/embedded/admin_html.cpp \
    ${srcdir}/../../console/embedded/admin_icon.cpp \
    ${srcdir}/../../console/embedded/amin_ecma.cpp \
    ${srcdir}/../../console/embedded/native_css.cpp \
    ${srcdir}/../../console/embedded/native_ecma.cpp \
    ${srcdir}/../../console/embedded/native_font.cpp \
    ${srcdir}/../../console/embedded/native_html.cpp \
    ${srcdir}/../../console/embedded/native_icon.cpp

target_console = console/bs

console: ${target_console}

endif WITH_CONSOLE
