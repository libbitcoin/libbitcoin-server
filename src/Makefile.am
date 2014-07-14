AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../include ${libbitcoin_CFLAGS} ${libczmqpp_LIBS} ${libconfigxx_CFLAGS} -DSYSCONFDIR=\"${sysconfdir}\"

lib_LTLIBRARIES = libobelisk.la
libobelisk_la_SOURCES = \
    message.cpp \
    client/backend.cpp \
    client/interface/client_interface.cpp \
    client/interface/client_fetch_x.cpp \
    client/interface/client_blockchain.cpp \
    client/interface/client_protocol.cpp \
    client/interface/client_transaction_pool.cpp

bin_PROGRAMS = obelisk-server

confdir=$(sysconfdir)/obelisk
conf_DATA = \
    worker/worker.cfg

obelisk_server_SOURCES = \
    worker/config.cpp \
    worker/echo.cpp \
    worker/main.cpp \
    worker/worker.cpp
#    worker/node_impl.cpp
#    worker/publisher.cpp
#    worker/subscribe_manager.cpp
#    worker/service/fullnode.cpp
#    worker/service/fetch_x.cpp
#    worker/service/blockchain.cpp
#    worker/service/protocol.cpp
#    worker/service/transaction_pool.cpp

obelisk_server_LDADD = -lobelisk ${libbitcoin_LIBS} ${libzmq_LIBS} ${libsodium_LIBS} ${libczmq_LIBS} ${libczmqpp_LIBS} ${libconfigxx_LIBS}

