AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = obelisk

obelisk_SOURCES = \
    message.cpp \
    config.cpp \
    echo.cpp \
    main.cpp \
    worker.cpp \
    node_impl.cpp \
    publisher.cpp \
    subscribe_manager.cpp \
    service/fullnode.cpp \
    service/fetch_x.cpp \
    service/blockchain.cpp \
    service/protocol.cpp \
    service/transaction_pool.cpp

obelisk_CPPFLAGS = \
    ${bitcoin_CFLAGS} \
    ${bitcoin_blockchain_CFLAGS} \
    ${sodium_LIBS} \
    ${czmqpp_LIBS} \
    ${configpp_LIBS} \
    -DSYSCONFDIR=\"${sysconfdir}\"

obelisk_LDADD = \
    -L$(srcdir)/../src/ \
    ${bitcoin_LIBS} \
    ${bitcoin_blockchain_LIBS} \
    ${sodium_LIBS} \
    ${czmqpp_LIBS} \
    ${configpp_LIBS}

confdir=${sysconfdir}/obelisk
conf_DATA = worker.cfg

