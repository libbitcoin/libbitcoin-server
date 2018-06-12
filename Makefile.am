###############################################################################
#  Copyright (c) 2014-2015 libbitcoin-server developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Automake settings.
#==============================================================================
# Look for macros in the m4 subdirectory.
#------------------------------------------------------------------------------
ACLOCAL_AMFLAGS = -I m4


# Distribute, make and install products.
#==============================================================================
# files => ${pkgconfigdir}
#------------------------------------------------------------------------------
pkgconfig_DATA = \
    libbitcoin-server.pc

# files => ${docdir}
#------------------------------------------------------------------------------
doc_DATA = \
    AUTHORS \
    COPYING \
    ChangeLog \
    INSTALL \
    NEWS \
    README

# src/libbitcoin-server.la => ${libdir}
#------------------------------------------------------------------------------
lib_LTLIBRARIES = src/libbitcoin-server.la
src_libbitcoin_server_la_CPPFLAGS = -I${srcdir}/include -I${srcdir}/src/web/external/mbedtls/include -DSYSCONFDIR=\"${sysconfdir}\" ${bitcoin_protocol_CPPFLAGS} ${bitcoin_node_CPPFLAGS}
src_libbitcoin_server_la_LIBADD = ${bitcoin_protocol_LIBS} ${bitcoin_node_LIBS}
src_libbitcoin_server_la_SOURCES = \
    src/configuration.cpp \
    src/parser.cpp \
    src/server_node.cpp \
    src/settings.cpp \
    src/interface/blockchain.cpp \
    src/interface/subscribe.cpp \
    src/interface/transaction_pool.cpp \
    src/interface/unsubscribe.cpp \
    src/messages/message.cpp \
    src/messages/route.cpp \
    src/messages/subscription.cpp \
    src/services/block_service.cpp \
    src/services/heartbeat_service.cpp \
    src/services/query_service.cpp \
    src/services/transaction_service.cpp \
    src/web/block_socket.cpp \
    src/web/heartbeat_socket.cpp \
    src/web/json_string.cpp \
    src/web/query_socket.cpp \
    src/web/socket.cpp \
    src/web/transaction_socket.cpp \
    src/web/utility.cpp \
    src/web/external/mbedtls/aes.c \
    src/web/external/mbedtls/aesni.c \
    src/web/external/mbedtls/arc4.c \
    src/web/external/mbedtls/asn1parse.c \
    src/web/external/mbedtls/asn1write.c \
    src/web/external/mbedtls/base64.c \
    src/web/external/mbedtls/bignum.c \
    src/web/external/mbedtls/blowfish.c \
    src/web/external/mbedtls/camellia.c \
    src/web/external/mbedtls/ccm.c \
    src/web/external/mbedtls/certs.c \
    src/web/external/mbedtls/cipher.c \
    src/web/external/mbedtls/cipher_wrap.c \
    src/web/external/mbedtls/cmac.c \
    src/web/external/mbedtls/ctr_drbg.c \
    src/web/external/mbedtls/debug.c \
    src/web/external/mbedtls/des.c \
    src/web/external/mbedtls/dhm.c \
    src/web/external/mbedtls/ecdh.c \
    src/web/external/mbedtls/ecdsa.c \
    src/web/external/mbedtls/ecjpake.c \
    src/web/external/mbedtls/ecp.c \
    src/web/external/mbedtls/ecp_curves.c \
    src/web/external/mbedtls/entropy.c \
    src/web/external/mbedtls/entropy_poll.c \
    src/web/external/mbedtls/error.c \
    src/web/external/mbedtls/gcm.c \
    src/web/external/mbedtls/havege.c \
    src/web/external/mbedtls/hmac_drbg.c \
    src/web/external/mbedtls/md.c \
    src/web/external/mbedtls/md2.c \
    src/web/external/mbedtls/md4.c \
    src/web/external/mbedtls/md5.c \
    src/web/external/mbedtls/md_wrap.c \
    src/web/external/mbedtls/memory_buffer_alloc.c \
    src/web/external/mbedtls/net_sockets.c \
    src/web/external/mbedtls/oid.c \
    src/web/external/mbedtls/padlock.c \
    src/web/external/mbedtls/pem.c \
    src/web/external/mbedtls/pk.c \
    src/web/external/mbedtls/pk_wrap.c \
    src/web/external/mbedtls/pkcs11.c \
    src/web/external/mbedtls/pkcs12.c \
    src/web/external/mbedtls/pkcs5.c \
    src/web/external/mbedtls/pkparse.c \
    src/web/external/mbedtls/pkwrite.c \
    src/web/external/mbedtls/platform.c \
    src/web/external/mbedtls/ripemd160.c \
    src/web/external/mbedtls/rsa.c \
    src/web/external/mbedtls/rsa_internal.c \
    src/web/external/mbedtls/sha1.c \
    src/web/external/mbedtls/sha256.c \
    src/web/external/mbedtls/sha512.c \
    src/web/external/mbedtls/ssl_cache.c \
    src/web/external/mbedtls/ssl_ciphersuites.c \
    src/web/external/mbedtls/ssl_cli.c \
    src/web/external/mbedtls/ssl_cookie.c \
    src/web/external/mbedtls/ssl_srv.c \
    src/web/external/mbedtls/ssl_ticket.c \
    src/web/external/mbedtls/ssl_tls.c \
    src/web/external/mbedtls/threading.c \
    src/web/external/mbedtls/timing.c \
    src/web/external/mbedtls/version.c \
    src/web/external/mbedtls/version_features.c \
    src/web/external/mbedtls/x509.c \
    src/web/external/mbedtls/x509_create.c \
    src/web/external/mbedtls/x509_crl.c \
    src/web/external/mbedtls/x509_crt.c \
    src/web/external/mbedtls/x509_csr.c \
    src/web/external/mbedtls/x509write_crt.c \
    src/web/external/mbedtls/x509write_csr.c \
    src/web/external/mbedtls/xtea.c \
    src/web/external/mbedtls/include/mbedtls/aes.h \
    src/web/external/mbedtls/include/mbedtls/aesni.h \
    src/web/external/mbedtls/include/mbedtls/arc4.h \
    src/web/external/mbedtls/include/mbedtls/asn1.h \
    src/web/external/mbedtls/include/mbedtls/asn1write.h \
    src/web/external/mbedtls/include/mbedtls/base64.h \
    src/web/external/mbedtls/include/mbedtls/bignum.h \
    src/web/external/mbedtls/include/mbedtls/blowfish.h \
    src/web/external/mbedtls/include/mbedtls/bn_mul.h \
    src/web/external/mbedtls/include/mbedtls/camellia.h \
    src/web/external/mbedtls/include/mbedtls/ccm.h \
    src/web/external/mbedtls/include/mbedtls/certs.h \
    src/web/external/mbedtls/include/mbedtls/check_config.h \
    src/web/external/mbedtls/include/mbedtls/cipher.h \
    src/web/external/mbedtls/include/mbedtls/cipher_internal.h \
    src/web/external/mbedtls/include/mbedtls/cmac.h \
    src/web/external/mbedtls/include/mbedtls/compat-1.3.h \
    src/web/external/mbedtls/include/mbedtls/config.h \
    src/web/external/mbedtls/include/mbedtls/ctr_drbg.h \
    src/web/external/mbedtls/include/mbedtls/debug.h \
    src/web/external/mbedtls/include/mbedtls/des.h \
    src/web/external/mbedtls/include/mbedtls/dhm.h \
    src/web/external/mbedtls/include/mbedtls/ecdh.h \
    src/web/external/mbedtls/include/mbedtls/ecdsa.h \
    src/web/external/mbedtls/include/mbedtls/ecjpake.h \
    src/web/external/mbedtls/include/mbedtls/ecp.h \
    src/web/external/mbedtls/include/mbedtls/ecp_internal.h \
    src/web/external/mbedtls/include/mbedtls/entropy.h \
    src/web/external/mbedtls/include/mbedtls/entropy_poll.h \
    src/web/external/mbedtls/include/mbedtls/error.h \
    src/web/external/mbedtls/include/mbedtls/gcm.h \
    src/web/external/mbedtls/include/mbedtls/havege.h \
    src/web/external/mbedtls/include/mbedtls/hmac_drbg.h \
    src/web/external/mbedtls/include/mbedtls/md.h \
    src/web/external/mbedtls/include/mbedtls/md2.h \
    src/web/external/mbedtls/include/mbedtls/md4.h \
    src/web/external/mbedtls/include/mbedtls/md5.h \
    src/web/external/mbedtls/include/mbedtls/md_internal.h \
    src/web/external/mbedtls/include/mbedtls/memory_buffer_alloc.h \
    src/web/external/mbedtls/include/mbedtls/net.h \
    src/web/external/mbedtls/include/mbedtls/net_sockets.h \
    src/web/external/mbedtls/include/mbedtls/oid.h \
    src/web/external/mbedtls/include/mbedtls/padlock.h \
    src/web/external/mbedtls/include/mbedtls/pem.h \
    src/web/external/mbedtls/include/mbedtls/pk.h \
    src/web/external/mbedtls/include/mbedtls/pk_internal.h \
    src/web/external/mbedtls/include/mbedtls/pkcs11.h \
    src/web/external/mbedtls/include/mbedtls/pkcs12.h \
    src/web/external/mbedtls/include/mbedtls/pkcs5.h \
    src/web/external/mbedtls/include/mbedtls/platform.h \
    src/web/external/mbedtls/include/mbedtls/platform_time.h \
    src/web/external/mbedtls/include/mbedtls/ripemd160.h \
    src/web/external/mbedtls/include/mbedtls/rsa.h \
    src/web/external/mbedtls/include/mbedtls/rsa_internal.h \
    src/web/external/mbedtls/include/mbedtls/sha1.h \
    src/web/external/mbedtls/include/mbedtls/sha256.h \
    src/web/external/mbedtls/include/mbedtls/sha512.h \
    src/web/external/mbedtls/include/mbedtls/ssl.h \
    src/web/external/mbedtls/include/mbedtls/ssl_cache.h \
    src/web/external/mbedtls/include/mbedtls/ssl_ciphersuites.h \
    src/web/external/mbedtls/include/mbedtls/ssl_cookie.h \
    src/web/external/mbedtls/include/mbedtls/ssl_internal.h \
    src/web/external/mbedtls/include/mbedtls/ssl_ticket.h \
    src/web/external/mbedtls/include/mbedtls/threading.h \
    src/web/external/mbedtls/include/mbedtls/timing.h \
    src/web/external/mbedtls/include/mbedtls/version.h \
    src/web/external/mbedtls/include/mbedtls/x509.h \
    src/web/external/mbedtls/include/mbedtls/x509_crl.h \
    src/web/external/mbedtls/include/mbedtls/x509_crt.h \
    src/web/external/mbedtls/include/mbedtls/x509_csr.h \
    src/web/external/mbedtls/include/mbedtls/xtea.h \
    src/web/external/mongoose/mongoose.c \
    src/web/external/mongoose/mongoose.h \
    src/workers/authenticator.cpp \
    src/workers/notification_worker.cpp \
    src/workers/query_worker.cpp

# local: test/libbitcoin-server-test
#------------------------------------------------------------------------------
if WITH_TESTS

TESTS = libbitcoin-server-test_runner.sh

check_PROGRAMS = test/libbitcoin-server-test
test_libbitcoin_server_test_CPPFLAGS = -I${srcdir}/include ${bitcoin_protocol_CPPFLAGS} ${bitcoin_node_CPPFLAGS}
test_libbitcoin_server_test_LDADD = src/libbitcoin-server.la ${boost_unit_test_framework_LIBS} ${bitcoin_protocol_LIBS} ${bitcoin_node_LIBS}
test_libbitcoin_server_test_SOURCES = \
    test/main.cpp \
    test/server.cpp \
    test/stress.sh

endif WITH_TESTS

# console/bs => ${bindir}
#------------------------------------------------------------------------------
if WITH_CONSOLE

bin_PROGRAMS = console/bs
console_bs_CPPFLAGS = -I${srcdir}/include ${bitcoin_protocol_CPPFLAGS} ${bitcoin_node_CPPFLAGS}
console_bs_LDADD = src/libbitcoin-server.la ${bitcoin_protocol_LIBS} ${bitcoin_node_LIBS}
console_bs_SOURCES = \
    console/executor.cpp \
    console/executor.hpp \
    console/main.cpp

endif WITH_CONSOLE

# files => ${includedir}/bitcoin
#------------------------------------------------------------------------------
include_bitcoindir = ${includedir}/bitcoin
include_bitcoin_HEADERS = \
    include/bitcoin/server.hpp

include_bitcoin_serverdir = ${includedir}/bitcoin/server
include_bitcoin_server_HEADERS = \
    include/bitcoin/server/configuration.hpp \
    include/bitcoin/server/define.hpp \
    include/bitcoin/server/parser.hpp \
    include/bitcoin/server/server_node.hpp \
    include/bitcoin/server/settings.hpp \
    include/bitcoin/server/version.hpp

include_bitcoin_server_interfacedir = ${includedir}/bitcoin/server/interface
include_bitcoin_server_interface_HEADERS = \
    include/bitcoin/server/interface/blockchain.hpp \
    include/bitcoin/server/interface/subscribe.hpp \
    include/bitcoin/server/interface/transaction_pool.hpp \
    include/bitcoin/server/interface/unsubscribe.hpp

include_bitcoin_server_messagesdir = ${includedir}/bitcoin/server/messages
include_bitcoin_server_messages_HEADERS = \
    include/bitcoin/server/messages/message.hpp \
    include/bitcoin/server/messages/route.hpp \
    include/bitcoin/server/messages/subscription.hpp

include_bitcoin_server_servicesdir = ${includedir}/bitcoin/server/services
include_bitcoin_server_services_HEADERS = \
    include/bitcoin/server/services/block_service.hpp \
    include/bitcoin/server/services/heartbeat_service.hpp \
    include/bitcoin/server/services/query_service.hpp \
    include/bitcoin/server/services/transaction_service.hpp

include_bitcoin_server_webdir = ${includedir}/bitcoin/server/web
include_bitcoin_server_web_HEADERS = \
    include/bitcoin/server/web/block_socket.hpp \
    include/bitcoin/server/web/heartbeat_socket.hpp \
    include/bitcoin/server/web/json_string.hpp \
    include/bitcoin/server/web/query_socket.hpp \
    include/bitcoin/server/web/socket.hpp \
    include/bitcoin/server/web/transaction_socket.hpp

include_bitcoin_server_workersdir = ${includedir}/bitcoin/server/workers
include_bitcoin_server_workers_HEADERS = \
    include/bitcoin/server/workers/authenticator.hpp \
    include/bitcoin/server/workers/notification_worker.hpp \
    include/bitcoin/server/workers/query_worker.hpp

# files => ${bash_completiondir}
#------------------------------------------------------------------------------
if BASH_COMPLETIONDIR

dist_bash_completion_DATA = \
    data/bs

endif BASH_COMPLETIONDIR

# files => ${sysconfdir}/libbitcoin
#------------------------------------------------------------------------------
sysconf_libbitcoindir = ${sysconfdir}/libbitcoin
sysconf_libbitcoin_DATA = \
    data/bs.cfg


# Custom make targets.
#==============================================================================
# make target: console
#------------------------------------------------------------------------------
target_console = \
    console/bs

console: ${target_console}

