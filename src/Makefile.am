AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libobelisk.la

AM_CPPFLAGS = \
    -I$(srcdir)/../include \
    ${bitcoin_CFLAGS} \
    ${bitcoin_blockchain_CFLAGS} \
    ${sodium_CFLAGS} \
    ${czmqpp_CFLAGS} \
    ${configpp_CFLAGS} \
    -DSYSCONFDIR=\"${sysconfdir}\"

libobelisk_la_SOURCES = \
    message.cpp \
    client/backend.cpp \
    client/interface/client_interface.cpp \
    client/interface/client_fetch_x.cpp \
    client/interface/client_blockchain.cpp \
    client/interface/client_protocol.cpp \
    client/interface/client_transaction_pool.cpp

libobelisk_la_LIBADD = \
    ${bitcoin_LIBS} \
    ${bitcoin_blockchain_LIBS} \
    ${sodium_LIBS} \
    ${czmqpp_LIBS} \
    ${configpp_LIBS}

bin_PROGRAMS = obelisk

obelisk_SOURCES = \
    worker/config.cpp \
    worker/echo.cpp \
    worker/main.cpp \
    worker/worker.cpp \
    worker/node_impl.cpp \
    worker/publisher.cpp \
    worker/subscribe_manager.cpp \
    worker/service/fullnode.cpp \
    worker/service/fetch_x.cpp \
    worker/service/blockchain.cpp \
    worker/service/protocol.cpp \
    worker/service/transaction_pool.cpp

obelisk_LDADD = \
    -L$(srcdir)/../src/ \
    -lobelisk \
    ${bitcoin_blockchain_LIBS} \
    ${sodium_LIBS} \
    ${czmqpp_LIBS} \
    ${configpp_LIBS}

confdir=${sysconfdir}/obelisk
conf_DATA = worker/worker.cfg
