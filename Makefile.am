###############################################################################
#  Copyright (c) 2014-2015 libbitcoin-server developers (see COPYING).
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
src_libbitcoin_server_la_CPPFLAGS = -I${srcdir}/include -DSYSCONFDIR=\"${sysconfdir}\" ${bitcoin_protocol_CPPFLAGS} ${bitcoin_node_CPPFLAGS}
src_libbitcoin_server_la_LIBADD = ${bitcoin_protocol_LIBS} ${bitcoin_node_LIBS}
src_libbitcoin_server_la_SOURCES = \
    src/configuration.cpp \
    src/parser.cpp \
    src/server_node.cpp \
    src/settings.cpp \
    src/endpoints/block_endpont.cpp \
    src/endpoints/heart_endpont.cpp \
    src/endpoints/query_endpont.cpp \
    src/endpoints/trans_endpont.cpp \
    src/interface/address.cpp \
    src/interface/blockchain.cpp \
    src/interface/protocol.cpp \
    src/interface/transaction_pool.cpp \
    src/messages/incoming.cpp \
    src/messages/outgoing.cpp \
    src/utility/address_notifier.cpp \
    src/utility/curve_authenticator.cpp \
    src/utility/fetch_helpers.cpp

# local: test/libbitcoin_server_test
#------------------------------------------------------------------------------
if WITH_TESTS

TESTS = libbitcoin_server_test_runner.sh

check_PROGRAMS = test/libbitcoin_server_test
test_libbitcoin_server_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_protocol_CPPFLAGS} ${bitcoin_node_CPPFLAGS}
test_libbitcoin_server_test_LDADD = src/libbitcoin-server.la ${boost_unit_test_framework_LIBS} ${bitcoin_protocol_LIBS} ${bitcoin_node_LIBS}
test_libbitcoin_server_test_SOURCES = \
    test/main.cpp \
    test/server.cpp \
    test/stress.sh

endif WITH_TESTS

# console/bs => ${bindir}
#------------------------------------------------------------------------------
if WITH_CONSOLE

bin_PROGRAMS = console/bs
console_bs_CPPFLAGS = -I${srcdir}/include ${bitcoin_protocol_CPPFLAGS} ${bitcoin_node_CPPFLAGS}
console_bs_LDADD = src/libbitcoin-server.la ${bitcoin_protocol_LIBS} ${bitcoin_node_LIBS}
console_bs_SOURCES = \
    console/executor.cpp \
    console/executor.hpp \
    console/main.cpp

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
    include/bitcoin/server/parser.hpp \
    include/bitcoin/server/server_node.hpp \
    include/bitcoin/server/settings.hpp \
    include/bitcoin/server/version.hpp

include_bitcoin_server_endpointsdir = ${includedir}/bitcoin/server/endpoints
include_bitcoin_server_endpoints_HEADERS = \
    include/bitcoin/server/endpoints/block_endpoint.hpp \
    include/bitcoin/server/endpoints/heart_endpoint.hpp \
    include/bitcoin/server/endpoints/query_endpoint.hpp \
    include/bitcoin/server/endpoints/trans_endpoint.hpp

include_bitcoin_server_interfacedir = ${includedir}/bitcoin/server/interface
include_bitcoin_server_interface_HEADERS = \
    include/bitcoin/server/interface/address.hpp \
    include/bitcoin/server/interface/blockchain.hpp \
    include/bitcoin/server/interface/protocol.hpp \
    include/bitcoin/server/interface/transaction_pool.hpp

include_bitcoin_server_messagesdir = ${includedir}/bitcoin/server/messages
include_bitcoin_server_messages_HEADERS = \
    include/bitcoin/server/messages/incoming.hpp \
    include/bitcoin/server/messages/outgoing.hpp

include_bitcoin_server_utilitydir = ${includedir}/bitcoin/server/utility
include_bitcoin_server_utility_HEADERS = \
    include/bitcoin/server/utility/address_notifier.hpp \
    include/bitcoin/server/utility/curve_authenticator.hpp \
    include/bitcoin/server/utility/fetch_helpers.hpp

# files => ${bash_completiondir}
#------------------------------------------------------------------------------
if BASH_COMPLETIONDIR

dist_bash_completion_DATA = \
    data/bs

endif BASH_COMPLETIONDIR

# files => ${sysconfdir}/libbitcoin
#------------------------------------------------------------------------------
sysconf_libbitcoindir = ${sysconfdir}/libbitcoin
sysconf_libbitcoin_DATA = \
    data/bs.cfg


# Custom make targets.
#==============================================================================
# make target: console
#------------------------------------------------------------------------------
target_console = \
    console/bs

console: ${target_console}

