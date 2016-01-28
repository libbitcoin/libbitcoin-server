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
src_libbitcoin_server_la_CPPFLAGS = -I${srcdir}/include -DSYSCONFDIR=\"${sysconfdir}\" ${bitcoin_node_CPPFLAGS} ${sodium_CPPFLAGS} ${czmq___CPPFLAGS}
src_libbitcoin_server_la_LIBADD = ${bitcoin_node_LIBS} ${sodium_LIBS} ${czmq___LIBS}
src_libbitcoin_server_la_SOURCES = \
    src/dispatch.cpp \
    src/parser.cpp \
    src/server_node.cpp \
    src/settings.cpp \
    src/messaging/incoming_message.cpp \
    src/messaging/outgoing_message.cpp \
    src/messaging/publisher.cpp \
    src/messaging/request_worker.cpp \
    src/messaging/send_worker.cpp \
    src/messaging/subscribe_manager.cpp \
    src/queries/blockchain.cpp \
    src/queries/protocol.cpp \
    src/queries/transaction_pool.cpp \
    src/queries/utility.cpp \
    src/queries/utility.hpp

# local: test/libbitcoin_server_test
#------------------------------------------------------------------------------
if WITH_TESTS

TESTS = libbitcoin_server_test_runner.sh

check_PROGRAMS = test/libbitcoin_server_test
test_libbitcoin_server_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_node_CPPFLAGS} ${sodium_CPPFLAGS} ${czmq___CPPFLAGS}
test_libbitcoin_server_test_LDADD = src/libbitcoin-server.la ${boost_unit_test_framework_LIBS} ${bitcoin_node_LIBS} ${sodium_LIBS} ${czmq___LIBS}
test_libbitcoin_server_test_SOURCES = \
    test/main.cpp \
    test/server.cpp \
    test/stress.sh

endif WITH_TESTS

# console/bs => ${bindir}
#------------------------------------------------------------------------------
if WITH_CONSOLE

bin_PROGRAMS = console/bs
console_bs_CPPFLAGS = -I${srcdir}/include ${bitcoin_node_CPPFLAGS} ${sodium_CPPFLAGS} ${czmq___CPPFLAGS}
console_bs_LDADD = src/libbitcoin-server.la ${bitcoin_node_LIBS} ${sodium_LIBS} ${czmq___LIBS}
console_bs_SOURCES = \
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
    include/bitcoin/server/dispatch.hpp \
    include/bitcoin/server/parser.hpp \
    include/bitcoin/server/server_node.hpp \
    include/bitcoin/server/settings.hpp \
    include/bitcoin/server/version.hpp

include_bitcoin_server_messagingdir = ${includedir}/bitcoin/server/messaging
include_bitcoin_server_messaging_HEADERS = \
    include/bitcoin/server/messaging/incoming_message.hpp \
    include/bitcoin/server/messaging/outgoing_message.hpp \
    include/bitcoin/server/messaging/publisher.hpp \
    include/bitcoin/server/messaging/request_worker.hpp \
    include/bitcoin/server/messaging/send_worker.hpp \
    include/bitcoin/server/messaging/subscribe_manager.hpp

include_bitcoin_server_queriesdir = ${includedir}/bitcoin/server/queries
include_bitcoin_server_queries_HEADERS = \
    include/bitcoin/server/queries/blockchain.hpp \
    include/bitcoin/server/queries/protocol.hpp \
    include/bitcoin/server/queries/transaction_pool.hpp

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

