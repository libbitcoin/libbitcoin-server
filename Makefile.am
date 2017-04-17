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
    src/interface/blockchain.cpp \
    src/interface/subscribe.cpp \
    src/interface/transaction_pool.cpp \
    src/interface/unsubscribe.cpp \
    src/messages/message.cpp \
    src/messages/route.cpp \
    src/messages/subscription.cpp \
    src/services/block_service.cpp \
    src/services/heartbeat_service.cpp \
    src/services/query_service.cpp \
    src/services/transaction_service.cpp \
    src/workers/authenticator.cpp \
    src/workers/notification_worker.cpp \
    src/workers/query_worker.cpp

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

include_bitcoin_server_interfacedir = ${includedir}/bitcoin/server/interface
include_bitcoin_server_interface_HEADERS = \
    include/bitcoin/server/interface/blockchain.hpp \
    include/bitcoin/server/interface/subscribe.hpp \
    include/bitcoin/server/interface/transaction_pool.hpp \
    include/bitcoin/server/interface/unsubscribe.hpp

include_bitcoin_server_messagesdir = ${includedir}/bitcoin/server/messages
include_bitcoin_server_messages_HEADERS = \
    include/bitcoin/server/messages/message.hpp \
    include/bitcoin/server/messages/route.hpp \
    include/bitcoin/server/messages/subscription.hpp

include_bitcoin_server_servicesdir = ${includedir}/bitcoin/server/services
include_bitcoin_server_services_HEADERS = \
    include/bitcoin/server/services/block_service.hpp \
    include/bitcoin/server/services/heartbeat_service.hpp \
    include/bitcoin/server/services/query_service.hpp \
    include/bitcoin/server/services/transaction_service.hpp

include_bitcoin_server_workersdir = ${includedir}/bitcoin/server/workers
include_bitcoin_server_workers_HEADERS = \
    include/bitcoin/server/workers/authenticator.hpp \
    include/bitcoin/server/workers/notification_worker.hpp \
    include/bitcoin/server/workers/query_worker.hpp

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

