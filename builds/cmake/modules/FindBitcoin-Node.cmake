###############################################################################
#  Copyright (c) 2014-2019 libbitcoin-server developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################
# FindBitcoin-Node
#
# Use this module by invoking find_package with the form::
#
#   find_package( Bitcoin-Node
#     [version]              # Minimum version
#     [REQUIRED]             # Fail with error if bitcoin-node is not found
#   )
#
#   Defines the following for use:
#
#   bitcoin_node_FOUND                    - true if headers and requested libraries were found
#   bitcoin_node_INCLUDE_DIRS             - include directories for bitcoin-node libraries
#   bitcoin_node_LIBRARY_DIRS             - link directories for bitcoin-node libraries
#   bitcoin_node_LIBRARIES                - bitcoin-node libraries to be linked
#   bitcoin_node_PKG                      - bitcoin-node pkg-config package specification.
#

if (MSVC)
    if ( Bitcoin-Node_FIND_REQUIRED )
        set( _bitcoin_node_MSG_STATUS "SEND_ERROR" )
    else ()
        set( _bitcoin_node_MSG_STATUS "STATUS" )
    endif()

    set( bitcoin_node_FOUND false )
    message( ${_bitcoin_node_MSG_STATUS} "MSVC environment detection for 'bitcoin-node' not currently supported." )
else ()
    # required
    if ( Bitcoin-Node_FIND_REQUIRED )
        set( _bitcoin_node_REQUIRED "REQUIRED" )
    endif()

    # quiet
    if ( Bitcoin-Node_FIND_QUIETLY )
        set( _bitcoin_node_QUIET "QUIET" )
    endif()

    # modulespec
    if ( Bitcoin-Node_FIND_VERSION_COUNT EQUAL 0 )
        set( _bitcoin_node_MODULE_SPEC "libbitcoin-node" )
    else ()
        if ( Bitcoin-Node_FIND_VERSION_EXACT )
            set( _bitcoin_node_MODULE_SPEC_OP "=" )
        else ()
            set( _bitcoin_node_MODULE_SPEC_OP ">=" )
        endif()

        set( _bitcoin_node_MODULE_SPEC "libbitcoin-node ${_bitcoin_node_MODULE_SPEC_OP} ${Bitcoin-Node_FIND_VERSION}" )
    endif()

    pkg_check_modules( bitcoin_node ${_bitcoin_node_REQUIRED} ${_bitcoin_node_QUIET} "${_bitcoin_node_MODULE_SPEC}" )
    set( bitcoin_node_PKG "${_bitcoin_node_MODULE_SPEC}" )
endif()
