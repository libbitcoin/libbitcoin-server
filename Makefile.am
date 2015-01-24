#
# documentation
#
doc_DATA = README INSTALL COPYING

#
# pkg-config
#
pkgconfig_DATA = libbitcoin-server.pc

# look for macros in the m4 subdirectory
ACLOCAL_AMFLAGS = -I m4

#
# console application
#
bin_PROGRAMS = bitcoin_server

bitcoin_server_SOURCES = \
    src/message.cpp \
    src/config.cpp \
    src/echo.cpp \
    src/main.cpp \
    src/server.cpp \
    src/settings.cpp \
    src/worker.cpp \
    src/node_impl.cpp \
    src/publisher.cpp \
    src/subscribe_manager.cpp \
    src/service/compat.cpp \
    src/service/fullnode.cpp \
    src/service/fetch_x.cpp \
    src/service/blockchain.cpp \
    src/service/protocol.cpp \
    src/service/transaction_pool.cpp

bitcoin_server_CPPFLAGS = \
    -I${srcdir}/include \
    ${sodium_CFLAGS} \
    ${czmq___CFLAGS} \
    ${bitcoin_node_CFLAGS}

bitcoin_server_LDADD = \
    ${sodium_LIBS} \
    ${czmq___LIBS} \
    ${bitcoin_node_LIBS}

#
# configuration file
#
# TODO: coordinate this subdirectory and file name with the
# source code so they stay in sync.
confdir=${sysconfdir}/libbitcoin
conf_DATA = server.cfg

