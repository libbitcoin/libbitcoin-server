###############################################################################
#  Copyright (c) 2014-2020 libbitcoin-server developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# libbitcoin-server project configuration.
#------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(libbitcoin-server LANGUAGES C CXX)

enable_testing()

list( APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/modules" )
include(CheckIncludeFiles)
include(CheckSymbolExists)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (MSVC)
  set( CANONICAL_LIB_NAME libbitcoin-server )
else ()
  set( CANONICAL_LIB_NAME bitcoin-server )
  find_package( PkgConfig REQUIRED )

  set( prefix "${CMAKE_PREFIX_PATH}" )
  set( exec_prefix "\${prefix}" )
  set( libdir "\${exec_prefix}/lib" )
  set( includedir "\${exec_prefix}/include" )

  set( PACKAGE_VERSION "4.0.0" )
  set( VERSION "${PACKAGE_VERSION}" )
endif ()

set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

# Add compiler options
#------------------------------------------------------------------------------
# Warn on all stuff.
add_compile_options( "-Wall" )

# Warn on extra stuff.
add_compile_options( "-Wextra" )

# Be really annoying.
add_compile_options( "-Wpedantic" )

# Disallow warning on style order of declarations.
add_compile_options( "-Wno-reorder" )

# Suppress warning for incomplete field initialization.
add_compile_options( "-Wno-missing-field-initializers" )

# Conform to style.
add_compile_options( "-Wno-missing-braces" )

# Ignore comments within comments or commenting of backslash extended lines.
add_compile_options( "-Wno-comment" )

# Conflict in stdlib under clang.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options( "-Wno-mismatched-tags" )
endif()

# Implement -Dbash-completiondir and output ${bash-completiondir} and declare bash-completiondir.
#------------------------------------------------------------------------------
set( bash-completiondir "no" CACHE BOOL "Install bash completion support, optionally specifying the directory. This option may require elevated permissions." )

# Implement -Dpkgconfigdir and output ${pkgconfigdir}.
#------------------------------------------------------------------------------
set( pkgconfigdir "${libdir}/pkgconfig" CACHE PATH "Path to pkgconfig directory." )

# Implement -Dwith-tests and declare with-tests.
#------------------------------------------------------------------------------
set( with-tests "yes" CACHE BOOL "Compile with unit tests." )

# Implement -Dwith-console and declare with-console.
#------------------------------------------------------------------------------
set( with-console "yes" CACHE BOOL "Compile console application." )

# Implement -Denable-ndebug and define NDEBUG.
#------------------------------------------------------------------------------
set( enable-ndebug "yes" CACHE BOOL "Compile without debug assertions." )

if (enable-ndebug)
    add_definitions( -DNDEBUG )
endif()

# Inherit -Denable-shared and define BOOST_ALL_DYN_LINK.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    add_definitions( -DBOOST_ALL_DYN_LINK )
endif()

# Find boost
#------------------------------------------------------------------------------
find_package( Boost 1.76.0 REQUIRED COMPONENTS
    unit_test_framework )

set( boost_unit_test_framework_LIBS "-lboost_unit_test_framework" )

if (enable-ndebug)
    set( Boost_LIBRARY_DIR "${Boost_LIBRARY_DIR_DEBUG}" )
else ()
    set( Boost_LIBRARY_DIR "${Boost_LIBRARY_DIR_RELEASE}" )
endif()

set( boost_CPPFLAGS "-I${Boost_INCLUDE_DIR}" )
set( boost_LDFLAGS "-L${Boost_LIBRARY_DIR}" )

# Find bash-completion
#------------------------------------------------------------------------------
if (bash-completiondir)
    find_package( Bash-Completion 2.0.0 REQUIRED )
endif()

# Find bitcoin-node
#------------------------------------------------------------------------------
find_package( Bitcoin-Node 4.0.0 REQUIRED )

# Find bitcoin-protocol
#------------------------------------------------------------------------------
find_package( Bitcoin-Protocol 4.0.0 REQUIRED )

# Define project common includes directories
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    include_directories( SYSTEM
        ${bitcoin_protocol_INCLUDE_DIRS}
        ${bitcoin_node_INCLUDE_DIRS} )
else()
    include_directories( SYSTEM
        ${bitcoin_protocol_STATIC_INCLUDE_DIRS}
        ${bitcoin_node_STATIC_INCLUDE_DIRS} )
endif()

# Define project common library directories
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    link_directories(
        ${bitcoin_protocol_LIBRARY_DIRS}
        ${bitcoin_node_LIBRARY_DIRS} )
else()
    link_directories(
        ${bitcoin_protocol_STATIC_LIBRARY_DIRS}
        ${bitcoin_node_STATIC_LIBRARY_DIRS} )
endif()

# Define project common libraries/linker flags.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    link_libraries(
        "-fstack-protector"
        "-fstack-protector-all"
        ${bitcoin_protocol_LIBRARIES}
        ${bitcoin_node_LIBRARIES} )
else()
    link_libraries(
        "-fstack-protector"
        "-fstack-protector-all"
        ${bitcoin_protocol_STATIC_LIBRARIES}
        ${bitcoin_node_STATIC_LIBRARIES} )
endif()

add_definitions(
    -DSYSCONFDIR=\"${sysconfdir}\" )

# Define ${CANONICAL_LIB_NAME} project.
#------------------------------------------------------------------------------
add_library( ${CANONICAL_LIB_NAME}
    "../../src/configuration.cpp"
    "../../src/parser.cpp"
    "../../src/server_node.cpp"
    "../../src/settings.cpp"
    "../../src/interface/blockchain.cpp"
    "../../src/interface/server.cpp"
    "../../src/interface/subscribe.cpp"
    "../../src/interface/transaction_pool.cpp"
    "../../src/interface/unsubscribe.cpp"
    "../../src/messages/message.cpp"
    "../../src/messages/route.cpp"
    "../../src/messages/subscription.cpp"
    "../../src/services/block_service.cpp"
    "../../src/services/heartbeat_service.cpp"
    "../../src/services/query_service.cpp"
    "../../src/services/transaction_service.cpp"
    "../../src/web/block_socket.cpp"
    "../../src/web/default_page_data.cpp"
    "../../src/web/heartbeat_socket.cpp"
    "../../src/web/query_socket.cpp"
    "../../src/web/transaction_socket.cpp"
    "../../src/workers/authenticator.cpp"
    "../../src/workers/notification_worker.cpp"
    "../../src/workers/query_worker.cpp" )

# ${CANONICAL_LIB_NAME} project specific include directories.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    target_include_directories( ${CANONICAL_LIB_NAME} PRIVATE
        "../../include"
        ${bitcoin_protocol_INCLUDE_DIRS}
        ${bitcoin_node_INCLUDE_DIRS} )
else()
    target_include_directories( ${CANONICAL_LIB_NAME} PRIVATE
        "../../include"
        ${bitcoin_protocol_STATIC_INCLUDE_DIRS}
        ${bitcoin_node_STATIC_INCLUDE_DIRS} )
endif()

target_include_directories( ${CANONICAL_LIB_NAME} PUBLIC
"../../include" )

# ${CANONICAL_LIB_NAME} project specific libraries/linker flags.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    target_link_libraries( ${CANONICAL_LIB_NAME}
        ${bitcoin_protocol_LIBRARIES}
        ${bitcoin_node_LIBRARIES} )
else()
    target_link_libraries( ${CANONICAL_LIB_NAME}
        ${bitcoin_protocol_STATIC_LIBRARIES}
        ${bitcoin_node_STATIC_LIBRARIES} )
endif()

# Define libbitcoin-server-test project.
#------------------------------------------------------------------------------
if (with-tests)
    add_executable( libbitcoin-server-test
        "../../test/latest-addrs.py"
        "../../test/main.cpp"
        "../../test/popular_addrs.py"
        "../../test/server.cpp"
        "../../test/stress.sh" )

    add_test( NAME libbitcoin-server-test COMMAND libbitcoin-server-test
            --run_test=*
            --show_progress=no
            --detect_memory_leak=0
            --report_level=no
            --build_info=yes )

#     libbitcoin-server-test project specific include directories.
#------------------------------------------------------------------------------
    target_include_directories( libbitcoin-server-test PRIVATE
        "../../include" )

#     libbitcoin-server-test project specific libraries/linker flags.
#------------------------------------------------------------------------------
    target_link_libraries( libbitcoin-server-test
        ${CANONICAL_LIB_NAME}
        ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} )

endif()

# Define bs project.
#------------------------------------------------------------------------------
if (with-console)
    add_executable( bs
        "../../console/executor.cpp"
        "../../console/executor.hpp"
        "../../console/libbitcoin.ico"
        "../../console/main.cpp" )

#     bs project specific include directories.
#------------------------------------------------------------------------------
    target_include_directories( bs PRIVATE
        "../../include" )

#     bs project specific libraries/linker flags.
#------------------------------------------------------------------------------
    target_link_libraries( bs
        ${CANONICAL_LIB_NAME} )

endif()

# Manage pkgconfig installation.
#------------------------------------------------------------------------------
configure_file(
  "../../libbitcoin-server.pc.in"
  "libbitcoin-server.pc" @ONLY )

install( FILES
    "${CMAKE_CURRENT_BINARY_DIR}/libbitcoin-server.pc"
    DESTINATION "${pkgconfigdir}" )

# Manage installation of docs.
#------------------------------------------------------------------------------
install( FILES
    "../../AUTHORS"
    "../../COPYING"
    "../../ChangeLog"
    "../../INSTALL"
    "../../NEWS"
    "../../README"
    DESTINATION share/doc/libbitcoin-server )

# Manage ${CANONICAL_LIB_NAME} installation.
#------------------------------------------------------------------------------
install( TARGETS ${CANONICAL_LIB_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include )

# Manage bs installation.
#------------------------------------------------------------------------------
if (with-console)
    install( TARGETS bs
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include )
endif()

# Manage include installation.
#------------------------------------------------------------------------------
install( DIRECTORY "../../include/bitcoin"
    DESTINATION include )

# Manage data installation for bash_completion prefixed product.
#------------------------------------------------------------------------------
if (bash-completiondir)
    install( FILES
        "../../data/bs"
        DESTINATION etc/libbitcoin )
endif()

# Manage data installation for sysconf prefixed product.
#------------------------------------------------------------------------------
if (with-console)
    install( FILES
        "../../data/bs.cfg"
        DESTINATION etc/libbitcoin )
endif()

