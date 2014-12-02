###############################################################################
# Copyright (c) 2011-2014 libbitcoin-server developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# Standard declarations.
#==============================================================================
# Declare the required version of autoconf.
AC_PREREQ([2.65])

# Process command-line arguments and perform initialization and verification.
AC_INIT([libbitcoin-server], [3.0.0], [eric@voskuil.org])

# Do compilation tests.
AC_LANG(C++)

# Specify the temporary directory for build tools.
AC_CONFIG_AUX_DIR([build-aux])

# Specify the directory of additional local Autoconf macros.
AC_CONFIG_MACRO_DIR([m4])

# Run macros for operation of generated Makefiles, enable non-recursive make.
# Unless [foreign] is specified standard GNU files will be required,
# specifically: AUTHORS, COPYING, INSTALL, NEWS, README and ChangeLog.
AM_INIT_AUTOMAKE([subdir-objects])

# Enable C and Posix extensions that may be disabled on certain platforms.
AC_USE_SYSTEM_EXTENSIONS

# Enable the archiver.
AM_PROG_AR

# Initialize libtool.
LT_INIT

# Determine C++ compiler to use.
AC_PROG_CXX

# Enable shared libraries if available, and static if they don't conflict.
AC_PROG_LIBTOOL

# Declare using the GNU C library (glibc).
AC_GNU_SOURCE

# Compute the canonical host-system type variable host, including host_os.
AC_CANONICAL_HOST

# Check for baseline language coverage in the compiler for the C++11 standard.
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])

# Enable silent rules option.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Check for pkg-config.
PKG_PROG_PKG_CONFIG

# Declare environment variables that affect the build.
#------------------------------------------------------------------------------
AC_ARG_VAR([CC], "C compiler to use, such as gcc or clang")
AC_ARG_VAR([CXX], "C++ compiler to use, such as g++ or clang++")
AC_ARG_VAR([PKG_CONFIG_PATH], "Additional directories for package discovery.")


# Process options.
#==============================================================================
# Implement --with-pkgconfigdir and output ${pkgconfigdir}.
#------------------------------------------------------------------------------
AC_MSG_CHECKING(--with-pkgconfigdir option)
AC_ARG_WITH([pkgconfigdir],
    AS_HELP_STRING([--with-pkgconfigdir=PATH],
        [Path to pkgconfig directory. @<:@default=${libdir}/pkgconfig@:>@]),
    [pkgconfigdir=$withval],
    [pkgconfigdir=${libdir}/pkgconfig])
AC_MSG_RESULT([$pkgconfigdir])
AC_SUBST([pkgconfigdir])

# Implement --with-tests and declare WITH_TESTS.
#------------------------------------------------------------------------------
AC_MSG_CHECKING(--with-tests option)
AC_ARG_WITH([tests],
    AS_HELP_STRING([--with-tests],
        [Compile with unit tests. @<:@default=yes@:>@]),
    [with_tests=$withval],
    [with_tests=yes])
AC_MSG_RESULT([$with_tests])
AM_CONDITIONAL([WITH_TESTS], [test x$with_tests != xno])

# Implement --enable-debug option and define DEBUG.
#------------------------------------------------------------------------------
AC_MSG_CHECKING(--enable-debug option)
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug],
        [Compile with debug assertions. @<:@default=no@:>@]),
    [enable_debug=$enableval],
    [enable_debug=no])
AC_MSG_RESULT([$enable_debug])
AS_IF([test x$enable_debug != xno], AC_DEFINE([DEBUG]))

# Implement --enable-testnet option and define ENABLE_TESTNET.
#------------------------------------------------------------------------------
AC_MSG_CHECKING(--enable-testnet option)
AC_ARG_ENABLE([testnet],
    AS_HELP_STRING([--enable-testnet],
        [Compile for testnet (deprecated). @<:@default=no@:>@]),
    [enable_testnet=$enableval],
    [enable_testnet=no])
AC_MSG_RESULT([$enable_testnet])
AS_IF([test x$enable_testnet != xno], AC_DEFINE([ENABLE_TESTNET]))

# Inherit --enable-shared option and define BOOST_TEST_DYN_LINK.
#------------------------------------------------------------------------------
AS_IF([test x$enable_shared != xno], AC_DEFINE([BOOST_TEST_DYN_LINK]))


# Check dependencies.
#==============================================================================
# Require Boost of at least version 1.49.0 if on linux.
#------------------------------------------------------------------------------
AS_CASE([${host_os}], [*linux*],
    [AX_BOOST_BASE([1.49.0], [], [AC_MSG_ERROR(
        [Boost 1.49.0 or later is required but was not found.])])])

# Require Boost of at least version 1.54.0 if on darwin.
#------------------------------------------------------------------------------
AS_CASE([${host_os}], [*darwin*],
    [AX_BOOST_BASE([1.54.0], [], [AC_MSG_ERROR(
        [Boost 1.54.0 or later is required but was not found.])])])

AS_IF([test x$with_tests = xyes], [AX_BOOST_UNIT_TEST_FRAMEWORK])

# Require bitcoin-protocol of at least version 2.0.0 and output ${bitcoin_protocol_CFLAGS} and ${bitcoin_protocol_LIBS}.
#------------------------------------------------------------------------------
PKG_CHECK_MODULES([bitcoin_protocol], [libbitcoin-protocol >= 2.0.0])


# Set flags.
#==============================================================================
# Warn on all stuff.
#------------------------------------------------------------------------------
AX_CHECK_COMPILE_FLAG([-Wall],
    [CXXFLAGS="$CXXFLAGS -Wall"])

# Warn on extra stuff.
#------------------------------------------------------------------------------
AX_CHECK_COMPILE_FLAG([-Wextra],
    [CXXFLAGS="$CXXFLAGS -Wextra"])

# Be really annoying.
#------------------------------------------------------------------------------
AX_CHECK_COMPILE_FLAG([-Wpedantic],
    [CXXFLAGS="$CXXFLAGS -Wpedantic"],
    [AX_CHECK_COMPILE_FLAG([-pedantic],
        [CXXFLAGS="$CXXFLAGS -pedantic"])])

# Conform to style.
#------------------------------------------------------------------------------
AX_CHECK_COMPILE_FLAG([-Wno-missing-braces],
    [CXXFLAGS="$CXXFLAGS -Wno-missing-braces"])

# Clean up Boost 1.49 headers (gcc).
#------------------------------------------------------------------------------
AX_CHECK_COMPILE_FLAG([-Wno-unused-local-typedefs],
    [CXXFLAGS="$CXXFLAGS -Wno-unused-local-typedefs"])

# Clean up protobuf headers.
#------------------------------------------------------------------------------
AX_CHECK_COMPILE_FLAG([-Wno-unused-parameter],
    [CXXFLAGS="$CXXFLAGS -Wno-unused-parameter"])

# Protect stack.
#------------------------------------------------------------------------------
AX_CHECK_COMPILE_FLAG([-fstack-protector],
    [AX_CHECK_LINK_FLAG([-fstack-protector],
        [CXXFLAGS="$CXXFLAGS -fstack-protector"])])

# Protect stack comprehensively.
#------------------------------------------------------------------------------
AX_CHECK_COMPILE_FLAG([-fstack-protector-all],
    [AX_CHECK_LINK_FLAG([-fstack-protector-all],
        [CXXFLAGS="$CXXFLAGS -fstack-protector-all"])])


# Process outputs into templates.
#==============================================================================
AC_CONFIG_FILES([Makefile libbitcoin-server.pc ])
AC_OUTPUT
