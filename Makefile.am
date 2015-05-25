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
    src/message.cpp \
    src/publisher.cpp \
    src/server_node.cpp \
    src/subscribe_manager.cpp \
    src/worker.cpp \
    src/config/config.cpp \
    src/config/endpoint.cpp \
    src/config/settings.cpp \
    src/service/blockchain.cpp \
    src/service/compat.cpp \
    src/service/fetch_x.cpp \
    src/service/protocol.cpp \
    src/service/transaction_pool.cpp

# local: test/libbitcoin_server_test
#------------------------------------------------------------------------------
if WITH_TESTS

TESTS = libbitcoin_server_test_runner.sh

check_PROGRAMS = test/libbitcoin_server_test
test_libbitcoin_server_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_node_CPPFLAGS} ${sodium_CPPFLAGS} ${czmq___CPPFLAGS}
test_libbitcoin_server_test_LDADD = src/libbitcoin-server.la ${boost_unit_test_framework_LIBS} ${bitcoin_node_LIBS} ${sodium_LIBS} ${czmq___LIBS}
test_libbitcoin_server_test_SOURCES = \
    test/endpoint.cpp \
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
    include/bitcoin/server/define.hpp \
    include/bitcoin/server/dispatch.hpp \
    include/bitcoin/server/message.hpp \
    include/bitcoin/server/publisher.hpp \
    include/bitcoin/server/server_node.hpp \
    include/bitcoin/server/subscribe_manager.hpp \
    include/bitcoin/server/version.hpp \
    include/bitcoin/server/worker.hpp

include_bitcoin_server_configdir = ${includedir}/bitcoin/server/config
include_bitcoin_server_config_HEADERS = \
    include/bitcoin/server/config/config.hpp \
    include/bitcoin/server/config/endpoint.hpp \
    include/bitcoin/server/config/settings.hpp

include_bitcoin_server_servicedir = ${includedir}/bitcoin/server/service
include_bitcoin_server_service_HEADERS = \
    include/bitcoin/server/service/blockchain.hpp \
    include/bitcoin/server/service/compat.hpp \
    include/bitcoin/server/service/fetch_x.hpp \
    include/bitcoin/server/service/protocol.hpp \
    include/bitcoin/server/service/transaction_pool.hpp \
    include/bitcoin/server/service/util.hpp

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

