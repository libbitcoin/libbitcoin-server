###############################################################################
#  Copyright (c) 2014-2015 libbitcoin-server developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Standard declarations.
#==============================================================================
# look for macros in the m4 subdirectory
ACLOCAL_AMFLAGS = -I m4


# Automake products.
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

# src/bitcoin_server => ${bindir}
#------------------------------------------------------------------------------
bin_PROGRAMS = src/bitcoin_server
src_bitcoin_server_CPPFLAGS = -I${srcdir}/include -DSYSCONFDIR=\"${sysconfdir}\" ${bitcoin_node_CPPFLAGS} ${sodium_CPPFLAGS} ${czmq___CPPFLAGS}
src_bitcoin_server_LDADD = ${bitcoin_node_LIBS} ${sodium_LIBS} ${czmq___LIBS}
src_bitcoin_server_SOURCES = \
    src/config.cpp \
    src/echo.cpp \
    src/endpoint.cpp \
    src/main.cpp \
    src/message.cpp \
    src/node_impl.cpp \
    src/publisher.cpp \
    src/server.cpp \
    src/settings.cpp \
    src/subscribe_manager.cpp \
    src/worker.cpp \
    src/service/blockchain.cpp \
    src/service/compat.cpp \
    src/service/fetch_x.cpp \
    src/service/fullnode.cpp \
    src/service/protocol.cpp \
    src/service/transaction_pool.cpp

# files => ${bash_completiondir}
#------------------------------------------------------------------------------
if BASH_COMPLETIONDIR

dist_bash_completion_DATA = \
    data/bitcoin_server

endif BASH_COMPLETIONDIR

# files => ${sysconfdir}/libbitcoin
#------------------------------------------------------------------------------
sysconf_libbitcoindir = ${sysconfdir}/libbitcoin
sysconf_libbitcoin_DATA = \
    data/server.cfg

