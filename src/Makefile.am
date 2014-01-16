AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../include ${libbitcoin_CFLAGS} -DSYSCONFDIR=\"${sysconfdir}\"

lib_LTLIBRARIES = libobelisk.la
libobelisk_la_SOURCES = \
    message.cpp \
    zmq_message.cpp \
    client/backend.cpp \
    client/interface/interface.cpp \
    client/interface/fetch_x.cpp \
    client/interface/blockchain.cpp \
    client/interface/protocol.cpp \
    client/interface/transaction_pool.cpp

bin_PROGRAMS = obbalancer obworker

confdir=$(sysconfdir)/obelisk
conf_DATA = \
    balancer/balancer.cfg \
    worker/worker.cfg

obbalancer_SOURCES = \
    balancer/balancer.cpp \
    balancer/config.cpp

obbalancer_LDADD = -lobelisk ${libbitcoin_LIBS} ${libzmq_LIBS} ${libconfigxx_LIBS}

obworker_SOURCES = \
    worker/config.cpp \
    worker/echo.cpp \
    worker/main.cpp \
    worker/node_impl.cpp \
    worker/publisher.cpp \
    worker/subscribe_manager.cpp \
    worker/worker.cpp \
    worker/service/fullnode.cpp \
    worker/service/fetch_x.cpp \
    worker/service/blockchain.cpp \
    worker/service/protocol.cpp \
    worker/service/transaction_pool.cpp

obworker_LDADD = -lobelisk ${libbitcoin_LIBS} ${libzmq_LIBS} ${libconfigxx_LIBS}

