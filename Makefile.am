doc_DATA = README INSTALL COPYING

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
    ${bitcoin_CFLAGS} \
    ${bitcoin_blockchain_CFLAGS} \
    ${bitcoin_node_CFLAGS} \
    ${sodium_LIBS} \
    ${czmqpp_LIBS} \
    ${configpp_LIBS} \
    -DSYSCONFDIR=\"${sysconfdir}\"

obelisk_LDADD = \
    ${bitcoin_LIBS} \
    ${bitcoin_blockchain_LIBS} \
    ${bitcoin_node_LIBS} \
    ${sodium_LIBS} \
    ${czmqpp_LIBS} \
    ${configpp_LIBS}

confdir=${sysconfdir}/obelisk
conf_DATA = worker.cfg

