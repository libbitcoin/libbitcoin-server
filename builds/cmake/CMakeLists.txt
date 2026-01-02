###############################################################################
#  Copyright (c) 2014-2025 libbitcoin-server developers (see COPYING).
#
#         GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
#
###############################################################################

# libbitcoin-server project configuration.
#------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

project(libbitcoin-server LANGUAGES C CXX)

enable_testing()

list( APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/modules" )
include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)
include(CheckLinkerFlag)

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

  set( CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib" )

  set( PACKAGE_VERSION "4.0.0" )
  set( VERSION "${PACKAGE_VERSION}" )
endif ()

set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

# Add compiler options
#------------------------------------------------------------------------------
# Warn on all stuff.
check_cxx_compiler_flag( "-Wall" HAS_FLAG_WALL )
if ( HAS_FLAG_WALL )
    add_compile_options( $<$<COMPILE_LANGUAGE:CXX>:-Wall> )
else()
    message( FATAL_ERROR "Compiler does not support -Wall" )
endif()

# Warn on extra stuff.
check_cxx_compiler_flag( "-Wextra" HAS_FLAG_WEXTRA )
if ( HAS_FLAG_WEXTRA )
    add_compile_options( $<$<COMPILE_LANGUAGE:CXX>:-Wextra> )
else()
    message( FATAL_ERROR "Compiler does not support -Wextra" )
endif()

# Disallow warning on style order of declarations.
check_cxx_compiler_flag( "-Wno-reorder" HAS_FLAG_WNO-REORDER )
if ( HAS_FLAG_WNO-REORDER )
    add_compile_options( $<$<COMPILE_LANGUAGE:CXX>:-Wno-reorder> )
else()
    message( FATAL_ERROR "Compiler does not support -Wno-reorder" )
endif()

# Suppress warning for incomplete field initialization.
check_cxx_compiler_flag( "-Wno-missing-field-initializers" HAS_FLAG_WNO-MISSING-FIELD-INITIALIZERS )
if ( HAS_FLAG_WNO-MISSING-FIELD-INITIALIZERS )
    add_compile_options( $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers> )
else()
    message( FATAL_ERROR "Compiler does not support -Wno-missing-field-initializers" )
endif()

# Conform to style.
check_cxx_compiler_flag( "-Wno-missing-braces" HAS_FLAG_WNO-MISSING-BRACES )
if ( HAS_FLAG_WNO-MISSING-BRACES )
    add_compile_options( $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-braces> )
else()
    message( FATAL_ERROR "Compiler does not support -Wno-missing-braces" )
endif()

# Ignore comments within comments or commenting of backslash extended lines.
check_cxx_compiler_flag( "-Wno-comment" HAS_FLAG_WNO-COMMENT )
if ( HAS_FLAG_WNO-COMMENT )
    add_compile_options( $<$<COMPILE_LANGUAGE:CXX>:-Wno-comment> )
else()
    message( FATAL_ERROR "Compiler does not support -Wno-comment" )
endif()

# Suppress warning for copy of implicitly generated copy constructor.
check_cxx_compiler_flag( "-Wno-deprecated-copy" HAS_FLAG_WNO-DEPRECATED-COPY )
if ( HAS_FLAG_WNO-DEPRECATED-COPY )
    add_compile_options( $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-copy> )
else()
    message( FATAL_ERROR "Compiler does not support -Wno-deprecated-copy" )
endif()

# Conflict in stdlib under clang.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    check_cxx_compiler_flag( "-Wno-mismatched-tags" HAS_FLAG_WNO-MISMATCHED-TAGS )
    if ( HAS_FLAG_WNO-MISMATCHED-TAGS )
        add_compile_options( $<$<COMPILE_LANGUAGE:CXX>:-Wno-mismatched-tags> )
    else()
        message( FATAL_ERROR "Compiler does not support -Wno-mismatched-tags" )
    endif()
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

if (BUILD_SHARED_LIBS)
    set( Boost_USE_STATIC_LIBS "off" )
else()
    set( Boost_USE_STATIC_LIBS "on" )
endif()

# Find boost
#------------------------------------------------------------------------------
find_package( Boost 1.86.0 REQUIRED COMPONENTS
    unit_test_framework )

set( boost_unit_test_framework_LIBS "-lboost_unit_test_framework" )

set( Boost_LIBRARY_DIR $<IF:$<BOOL:${enable-ndebug}>,"${Boost_LIBRARY_DIR_RELEASE}","${Boost_LIBRARY_DIR_DEBUG}">)

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

# Define project common includes for build.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    set( bitcoin_protocol_FOR_BUILD_INCLUDE_DIRS ${bitcoin_protocol_INCLUDE_DIRS} CACHE STRING "Placeholder" )
    set( bitcoin_node_FOR_BUILD_INCLUDE_DIRS ${bitcoin_node_INCLUDE_DIRS} CACHE STRING "Placeholder" )
else()
    set( bitcoin_protocol_FOR_BUILD_INCLUDE_DIRS ${bitcoin_protocol_STATIC_INCLUDE_DIRS} CACHE STRING "Placeholder" )
    set( bitcoin_node_FOR_BUILD_INCLUDE_DIRS ${bitcoin_node_STATIC_INCLUDE_DIRS} CACHE STRING "Placeholder" )
endif()

# Define project common includes directories
#------------------------------------------------------------------------------
include_directories( SYSTEM
    ${bitcoin_protocol_FOR_BUILD_INCLUDE_DIRS}
    ${bitcoin_node_FOR_BUILD_INCLUDE_DIRS} )

# Define project common library directories for build.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    set( bitcoin_protocol_FOR_BUILD_LIBRARY_DIRS ${bitcoin_protocol_LIBRARY_DIRS} CACHE STRING "Placeholder" )
    set( bitcoin_node_FOR_BUILD_LIBRARY_DIRS ${bitcoin_node_LIBRARY_DIRS} CACHE STRING "Placeholder" )
else()
    set( bitcoin_protocol_FOR_BUILD_LIBRARY_DIRS ${bitcoin_protocol_STATIC_LIBRARY_DIRS} CACHE STRING "Placeholder" )
    set( bitcoin_node_FOR_BUILD_LIBRARY_DIRS ${bitcoin_node_STATIC_LIBRARY_DIRS} CACHE STRING "Placeholder" )
endif()

# Define project common library directories
#------------------------------------------------------------------------------
link_directories(
    ${bitcoin_protocol_FOR_BUILD_LIBRARY_DIRS}
    ${bitcoin_node_FOR_BUILD_LIBRARY_DIRS} )

# Define project common linker flags.
#------------------------------------------------------------------------------
check_linker_flag(CXX "-no_fixup_chains" "no_fixup_chains_FOUND")

if (no_fixup_chains_FOUND)
    add_link_options("-no_fixup_chains")
endif()

check_linker_flag(CXX "-fstack-protector" "fstack_protector_FOUND")

if (fstack_protector_FOUND)
    add_link_options("-fstack-protector")
endif()

check_linker_flag(CXX "-fstack-protector-all" "fstack_protector_all_FOUND")

if (fstack_protector_all_FOUND)
    add_link_options("-fstack-protector-all")
endif()


# Define common library usage for build.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    set( bitcoin_protocol_FOR_BUILD_LIBRARIES ${bitcoin_protocol_LIBRARIES} CACHE STRING "Placeholder" )
    set( bitcoin_node_FOR_BUILD_LIBRARIES ${bitcoin_node_LIBRARIES} CACHE STRING "Placeholder" )
else()
    set( bitcoin_protocol_FOR_BUILD_LIBRARIES ${bitcoin_protocol_STATIC_LIBRARIES} CACHE STRING "Placeholder" )
    set( bitcoin_node_FOR_BUILD_LIBRARIES ${bitcoin_node_STATIC_LIBRARIES} CACHE STRING "Placeholder" )
endif()

# Define project common libraries.
#------------------------------------------------------------------------------
link_libraries(
    ${bitcoin_protocol_FOR_BUILD_LIBRARIES}
    ${bitcoin_node_FOR_BUILD_LIBRARIES} )

add_definitions(
    -DSYSCONFDIR=\"${sysconfdir}\" )

# Define ${CANONICAL_LIB_NAME} project.
#------------------------------------------------------------------------------
add_library( ${CANONICAL_LIB_NAME}
    "../../src/configuration.cpp"
    "../../src/error.cpp"
    "../../src/parser.cpp"
    "../../src/server_node.cpp"
    "../../src/settings.cpp"
    "../../src/parsers/bitcoind_query.cpp"
    "../../src/parsers/bitcoind_target.cpp"
    "../../src/parsers/explore_query.cpp"
    "../../src/parsers/explore_target.cpp"
    "../../src/protocols/protocol_bitcoind_rest.cpp"
    "../../src/protocols/protocol_bitcoind_rpc.cpp"
    "../../src/protocols/protocol_electrum.cpp"
    "../../src/protocols/protocol_electrum_version.cpp"
    "../../src/protocols/protocol_explore.cpp"
    "../../src/protocols/protocol_html.cpp"
    "../../src/protocols/protocol_http.cpp"
    "../../src/protocols/protocol_stratum_v1.cpp" )

# ${CANONICAL_LIB_NAME} project specific include directory normalization for build.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    set( bitcoin_protocol_FOR_BUILD_INCLUDE_DIRS ${bitcoin_protocol_INCLUDE_DIRS} CACHE STRING "Placeholder" )
    set( bitcoin_node_FOR_BUILD_INCLUDE_DIRS ${bitcoin_node_INCLUDE_DIRS} CACHE STRING "Placeholder" )
else()
    set( bitcoin_protocol_FOR_BUILD_INCLUDE_DIRS ${bitcoin_protocol_STATIC_INCLUDE_DIRS} CACHE STRING "Placeholder" )
    set( bitcoin_node_FOR_BUILD_INCLUDE_DIRS ${bitcoin_node_STATIC_INCLUDE_DIRS} CACHE STRING "Placeholder" )
endif()

# ${CANONICAL_LIB_NAME} project specific include directories.
#------------------------------------------------------------------------------
target_include_directories( ${CANONICAL_LIB_NAME} PRIVATE
    "../../include"
    ${bitcoin_protocol_FOR_BUILD_INCLUDE_DIRS}
    ${bitcoin_node_FOR_BUILD_INCLUDE_DIRS} )

target_include_directories( ${CANONICAL_LIB_NAME} PUBLIC
"../../include" )

# ${CANONICAL_LIB_NAME} project specific libraries noramalization for build.
#------------------------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    set( bitcoin_protocol_FOR_BUILD_LIBRARIES ${bitcoin_protocol_LIBRARIES} CACHE STRING "Placeholder" )
    set( bitcoin_node_FOR_BUILD_LIBRARIES ${bitcoin_node_LIBRARIES} CACHE STRING "Placeholder" )
else()
    set( bitcoin_protocol_FOR_BUILD_LIBRARIES ${bitcoin_protocol_STATIC_LIBRARIES} CACHE STRING "Placeholder" )
    set( bitcoin_node_FOR_BUILD_LIBRARIES ${bitcoin_node_STATIC_LIBRARIES} CACHE STRING "Placeholder" )
endif()

# ${CANONICAL_LIB_NAME} project specific libraries/linker flags.
#------------------------------------------------------------------------------
target_link_libraries( ${CANONICAL_LIB_NAME}
    ${bitcoin_protocol_FOR_BUILD_LIBRARIES}
    ${bitcoin_node_FOR_BUILD_LIBRARIES} )

# Define libbitcoin-server-test project.
#------------------------------------------------------------------------------
if (with-tests)
    add_executable( libbitcoin-server-test
        "../../test/error.cpp"
        "../../test/main.cpp"
        "../../test/test.cpp"
        "../../test/test.hpp" )

    add_test( NAME libbitcoin-server-test COMMAND libbitcoin-server-test
            --run_test=*
            --log_level=warning
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
        "../../console/executor_commands.cpp"
        "../../console/executor_dumps.cpp"
        "../../console/executor_events.cpp"
        "../../console/executor_logging.cpp"
        "../../console/executor_options.cpp"
        "../../console/executor_runner.cpp"
        "../../console/executor_scans.cpp"
        "../../console/executor_store.cpp"
        "../../console/executor_test_reader.cpp"
        "../../console/executor_test_writer.cpp"
        "../../console/libbitcoin.ico"
        "../../console/localize.hpp"
        "../../console/main.cpp"
        "../../console/stack_trace.cpp"
        "../../console/stack_trace.hpp"
        "../../console/embedded/embedded.hpp"
        "../../console/embedded/explore_css.cpp"
        "../../console/embedded/explore_ecma.cpp"
        "../../console/embedded/explore_font.cpp"
        "../../console/embedded/explore_html.cpp"
        "../../console/embedded/explore_icon.cpp"
        "../../console/embedded/web_css.cpp"
        "../../console/embedded/web_ecma.cpp"
        "../../console/embedded/web_font.cpp"
        "../../console/embedded/web_html.cpp"
        "../../console/embedded/web_icon.cpp" )

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

