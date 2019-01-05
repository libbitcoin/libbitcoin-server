###############################################################################
#  Copyright (c) 2014-2019 libbitcoin-server developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################
# FindBitcoin-Protocol
#
# Use this module by invoking find_package with the form::
#
#   find_package( Bitcoin-Protocol
#     [version]              # Minimum version
#     [REQUIRED]             # Fail with error if bitcoin-protocol is not found
#   )
#
#   Defines the following for use:
#
#   bitcoin_protocol_FOUND                - true if headers and requested libraries were found
#   bitcoin_protocol_INCLUDE_DIRS         - include directories for bitcoin-protocol libraries
#   bitcoin_protocol_LIBRARY_DIRS         - link directories for bitcoin-protocol libraries
#   bitcoin_protocol_LIBRARIES            - bitcoin-protocol libraries to be linked
#   bitcoin_protocol_PKG                  - bitcoin-protocol pkg-config package specification.
#

if (MSVC)
    if ( Bitcoin-Protocol_FIND_REQUIRED )
        set( _bitcoin_protocol_MSG_STATUS "SEND_ERROR" )
    else ()
        set( _bitcoin_protocol_MSG_STATUS "STATUS" )
    endif()

    set( bitcoin_protocol_FOUND false )
    message( ${_bitcoin_protocol_MSG_STATUS} "MSVC environment detection for 'bitcoin-protocol' not currently supported." )
else ()
    # required
    if ( Bitcoin-Protocol_FIND_REQUIRED )
        set( _bitcoin_protocol_REQUIRED "REQUIRED" )
    endif()

    # quiet
    if ( Bitcoin-Protocol_FIND_QUIETLY )
        set( _bitcoin_protocol_QUIET "QUIET" )
    endif()

    # modulespec
    if ( Bitcoin-Protocol_FIND_VERSION_COUNT EQUAL 0 )
        set( _bitcoin_protocol_MODULE_SPEC "libbitcoin-protocol" )
    else ()
        if ( Bitcoin-Protocol_FIND_VERSION_EXACT )
            set( _bitcoin_protocol_MODULE_SPEC_OP "=" )
        else ()
            set( _bitcoin_protocol_MODULE_SPEC_OP ">=" )
        endif()

        set( _bitcoin_protocol_MODULE_SPEC "libbitcoin-protocol ${_bitcoin_protocol_MODULE_SPEC_OP} ${Bitcoin-Protocol_FIND_VERSION}" )
    endif()

    pkg_check_modules( bitcoin_protocol ${_bitcoin_protocol_REQUIRED} ${_bitcoin_protocol_QUIET} "${_bitcoin_protocol_MODULE_SPEC}" )
    set( bitcoin_protocol_PKG "${_bitcoin_protocol_MODULE_SPEC}" )
endif()
