AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = ${libbitcoin_CFLAGS} -I$(srcdir)/../include -DSYSCONFDIR=\"${sysconfdir}\"

lib_LTLIBRARIES = libobelisk.la
libobelisk_la_SOURCES = \
    message.cpp \
    zmq_message.cpp \
    client/backend.cpp \
    client/interface/interface.cpp \
    client/interface/fetch_history.cpp \
    client/interface/blockchain.cpp

bin_PROGRAMS = obbalancer obworker

confdir=$(sysconfdir)/obelisk
conf_DATA = \
    balancer/balancer.cfg \
    worker/worker.cfg

obbalancer_SOURCES = \
    balancer/balancer.cpp \
    balancer/config.cpp
obbalancer_LDADD = ${libbitcoin_LIBS} ${libzmq_LIBS} ${libconfigxx_LIBS}

obworker_SOURCES = \
    worker/config.cpp \
    worker/echo.cpp \
    worker/main.cpp \
    worker/node_impl.cpp \
    worker/publisher.cpp \
    worker/worker.cpp \
    worker/service/fullnode.cpp \
    worker/service/fetch_history.cpp \
    worker/service/blockchain.cpp

obworker_LDADD = ${libbitcoin_LIBS} ${libzmq_LIBS} ${libconfigxx_LIBS} -lobelisk

