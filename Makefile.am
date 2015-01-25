

#
# documentation
#
doc_DATA = README INSTALL COPYING

#
# pkg-config
#
pkgconfig_DATA = libbitcoin-server.pc

#
# bash completion
#
if BASH_COMPLETION_DIR
bashcompletiondir = ${bash_completion_dir}
dist_bashcompletion_DATA = data/bitcoin_server
endif

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
    ${bitcoin_node_CFLAGS} \
    -DSYSCONFDIR=\"${sysconfdir}\"

bitcoin_server_LDADD = \
    ${sodium_LIBS} \
    ${czmq___LIBS} \
    ${bitcoin_node_LIBS}

#
# configuration file
#
# TODO: coordinate this dir and file name with source code to keep in sync.
# Note that in a prefix install this file is placed relative to the prefix but
# execution of the server will default to reading in the system-wide directory.
confdir="${sysconfdir}/libbitcoin"
conf_DATA = data/server.cfg

