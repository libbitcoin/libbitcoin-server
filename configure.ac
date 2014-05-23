AC_INIT([obelisk], [0.1], [genjix@riseup.net])
AC_USE_SYSTEM_EXTENSIONS
AC_LANG(C++)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror])
AM_PROG_AR
LT_INIT
AC_PROG_CXX
AC_GNU_SOURCE
AX_CXX_COMPILE_STDCXX_11(noext,mandatory)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([libbitcoin], [libbitcoin])
PKG_CHECK_MODULES([libzmq], [libzmq])
PKG_CHECK_MODULES([libsodium], [libsodium])
PKG_CHECK_MODULES([libczmq], [libczmq])
PKG_CHECK_MODULES([libczmqpp], [libczmq++])
PKG_CHECK_MODULES([libconfigxx], [libconfig++])

AC_ARG_WITH([pkgconfigdir], AS_HELP_STRING([--with-pkgconfigdir=PATH],
    [Path to the pkgconfig directory [[LIBDIR/pkgconfig]]]),
    [pkgconfigdir="$withval"], [pkgconfigdir='${libdir}/pkgconfig'])
AC_SUBST([pkgconfigdir])

AC_CONFIG_FILES([Makefile include/obelisk/Makefile src/Makefile libobelisk.pc])
AC_DEFINE(LEVELDB_ENABLED)
AC_OUTPUT

