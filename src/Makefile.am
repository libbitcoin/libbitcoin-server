AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libobelisk.la
AM_CPPFLAGS = ${libbitcoin_CFLAGS} -I$(srcdir)/../include
libobelisk_la_SOURCES = \
    message.cpp \
    zmq_message.cpp

bin_PROGRAMS = obbalancer

obbalancer_SOURCES = \
    balancer/balancer.cpp \
    balancer/config.cpp
obbalancer_LDADD = ${libbitcoin_LIBS} ${libzmq_LIBS} ${libconfigxx_LIBS}

