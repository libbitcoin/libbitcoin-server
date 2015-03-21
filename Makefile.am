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


# Distribution, build and install products.
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
    src/config.hpp \
    src/echo.cpp \
    src/echo.hpp \
    src/endpoint.cpp \
    src/endpoint.hpp \
    src/main.cpp \
    src/message.cpp \
    src/message.hpp \
    src/node_impl.cpp \
    src/node_impl.hpp \
    src/publisher.cpp \
    src/publisher.hpp \
    src/server.cpp \
    src/server.hpp \
    src/settings.cpp \
    src/settings.hpp \
    src/subscribe_manager.cpp \
    src/subscribe_manager.hpp \
    src/version.hpp \
    src/worker-output.sh \
    src/worker.cpp \
    src/worker.hpp \
    src/service/blockchain.cpp \
    src/service/blockchain.hpp \
    src/service/compat.cpp \
    src/service/compat.hpp \
    src/service/fetch_x.cpp \
    src/service/fetch_x.hpp \
    src/service/fullnode.cpp \
    src/service/fullnode.hpp \
    src/service/protocol.cpp \
    src/service/protocol.hpp \
    src/service/transaction_pool.cpp \
    src/service/transaction_pool.hpp \
    src/service/util.hpp

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

