doc_DATA = README INSTALL COPYING

pkgconfig_DATA = libbitcoin-server.pc

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = obelisk

obelisk_SOURCES = \
    src/message.cpp \
    src/config.cpp \
    src/echo.cpp \
    src/main.cpp \
    src/worker.cpp \
    src/node_impl.cpp \
    src/publisher.cpp \
    src/subscribe_manager.cpp \
    src/service/fullnode.cpp \
    src/service/fetch_x.cpp \
    src/service/blockchain.cpp \
    src/service/protocol.cpp \
    src/service/transaction_pool.cpp

obelisk_CPPFLAGS = \
    ${config___CFLAGS} \
    ${sodium_CFLAGS} \
    ${czmq___CFLAGS} \
    ${bitcoin_node_CFLAGS} \
    -DSYSCONFDIR=\"${sysconfdir}\"

obelisk_LDADD = \
    ${config___LIBS} \
    ${sodium_LIBS} \
    ${czmq___LIBS} \
    ${bitcoin_node_LIBS}

confdir=${sysconfdir}/obelisk
conf_DATA = worker.cfg

