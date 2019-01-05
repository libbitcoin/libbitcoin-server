###############################################################################
#  Copyright (c) 2014-2019 libbitcoin developers (see COPYING).
#
###############################################################################
# FindMbedtls
#
# Use this module by invoking find_package with the form::
#
#   find_package( Mbedtls
#     [version]           # Minimum version (ignored)
#     [REQUIRED]          # Fail with error if mbedtls is not found
#   )
#
#   Defines the following for use:
#
#   mbedtls_FOUND         - true if headers and requested libraries were found
#   mbedtls_LIBRARIES     - mbedtls libraries to be linked
#   mbedtls_LIBS          - mbedtls libraries to be linked
#

if (DEFINED Mbedtls_FIND_VERSION)
    message( SEND_ERROR
        "Library 'mbedtls' unable to process version: ${Mbedtls_FIND_VERSION}" )
endif()


if (MSVC)
    set( mbedtls_FOUND false )
    message( STATUS
        "MSVC environment detection for 'mbedtls' not currently supported." )
else ()
    # conditionally include static library suffix
    if (BUILD_SHARED_LIBS)
        set( _mbedcrypto_lib_name "mbedcrypto" )
        set( _mbedtls_lib_name    "mbedtls" )
        set( _mbedx509_lib_name   "mbedx509" )
    else ()
        set( _mbedcrypto_lib_name "libmbedcrypto.a" )
        set( _mbedtls_lib_name    "libmbedtls.a" )
        set( _mbedx509_lib_name   "libmbedx509.a" )
    endif()

    find_library( tls_LIBRARIES     "${_mbedtls_lib_name}" )
    find_library( crypto_LIBRARIES  "${_mbedcrypto_lib_name}" )
    find_library( x509_LIBRARIES    "${_mbedx509_lib_name}" )

    if (tls_LIBRARIES-NOTFOUND OR crypto_LIBRARIES-NOTFOUND OR x509_LIBRARIES-NOTFOUND)
        set( mbedtls_FOUND false )
    else ()
        set( mbedtls_FOUND true )
        set( mbedtls_LIBRARIES "${tls_LIBRARIES};${crypto_LIBRARIES};${x509_LIBRARIES}" )
        set( mbedtls_LIBS "-lmbedtls -lmbedcrypto -lmbedx509" )
    endif()

    if (NOT BUILD_SHARED_LIBS)
        set( mbedtls_STATIC_LIBRARIES "${mbedtls_LIBRARIES}" )
    endif()
endif()


if (NOT mbedtls_FOUND)
    if ( Mbedtls_FIND_REQUIRED )
        set( _mbedtls_MSG_STATUS "SEND_ERROR" )
    else ()
        set( _mbedtls_MSG_STATUS "STATUS" )
    endif()

    message( _mbedtls_MSG_STATUS "Library 'mbedtls'  not found (${_mbedtls_MSG_STATUS})." )
else ()
    message( "Library 'mbedtls' found." )
endif()
