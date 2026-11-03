#!/bin/bash
###############################################################################
#  Copyright (c) 2014-2026 libbitcoin-server developers (see COPYING).
#  store_path and the cross search paths derive from Bitcoin Core
#  contrib/guix/libexec, Copyright (c) The Bitcoin Core developers,
#  MIT license (https://opensource.org/license/mit).
###############################################################################
# Build one release binary inside the Guix container.
# environment: HOST, BASENAME, PROJECT, PROGRAM, FLAGS, LINKFLAGS, OPTIONS, JOBS
set -euo pipefail

export LC_ALL=C.UTF-8
export TZ=UTC
export HOME=/tmp
umask 0022

store_path()
{
    grep --extended-regexp "/[^-]{32}-${1}-[^-]+${2:+-${2}}" "${GUIX_ENVIRONMENT}/manifest" \
        | head --lines=1 \
        | sed --expression='s|\x29*$||' \
              --expression='s|^[[:space:]]*"||' \
              --expression='s|"[[:space:]]*$||'
}

dynamic_linker()
{
    case "${HOST}" in
        x86_64-linux-gnu)  echo /lib64/ld-linux-x86-64.so.2 ;;
        aarch64-linux-gnu) echo /lib/ld-linux-aarch64.so.1 ;;
        *) echo "Unsupported host '${HOST}'." >&2; exit 1 ;;
    esac
}

CROSS_GLIBC="$(store_path "glibc-cross-${HOST}")"
CROSS_GLIBC_STATIC="$(store_path "glibc-cross-${HOST}" static)"
CROSS_KERNEL="$(store_path "linux-libre-headers-cross-${HOST}")"
CROSS_GCC="$(store_path "gcc-cross-${HOST}")"
CROSS_GCC_LIB_STORE="$(store_path "gcc-cross-${HOST}" lib)"
CROSS_GCC_LIBS=( "${CROSS_GCC_LIB_STORE}/lib/gcc/${HOST}"/* )
CROSS_GCC_LIB="${CROSS_GCC_LIBS[0]}"

export CROSS_C_INCLUDE_PATH="${CROSS_GCC_LIB}/include:${CROSS_GCC_LIB}/include-fixed:${CROSS_GLIBC}/include:${CROSS_KERNEL}/include"
export CROSS_CPLUS_INCLUDE_PATH="${CROSS_GCC}/include/c++:${CROSS_GCC}/include/c++/${HOST}:${CROSS_GCC}/include/c++/backward:${CROSS_C_INCLUDE_PATH}"
export CROSS_LIBRARY_PATH="${CROSS_GCC_LIB_STORE}/lib:${CROSS_GCC_LIB}:${CROSS_GLIBC}/lib:${CROSS_GLIBC_STATIC}/lib"
export GUIX_LD_WRAPPER_DISABLE_RPATH=yes

export CC="${HOST}-gcc"
export CXX="${HOST}-g++"

STORE_MAP="$(find /gnu/store -maxdepth 1 -mindepth 1 -type d -exec echo -n " -ffile-prefix-map={}=/usr" \;)"
export CFLAGS="${FLAGS} -g -ffile-prefix-map=/distsrc=. -gno-record-gcc-switches${STORE_MAP}"
export CXXFLAGS="${CFLAGS}"
export LDFLAGS="${LINKFLAGS} -Wl,--build-id -Wl,--dynamic-linker=$(dynamic_linker)"

bash "/distsrc/${PROJECT}/builds/cmake/install-cmake.sh" \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR="${HOST%%-*}" \
    -DCMAKE_SKIP_INSTALL_RPATH=ON \
    -DCMAKE_C_STANDARD_LIBRARIES=-Wl,-lpthread,-ldl \
    -DCMAKE_CXX_STANDARD_LIBRARIES=-Wl,-lpthread,-ldl \
    --noninteractive \
    --build-use-local-src \
    --build-src-dir=/distsrc \
    --prefix=/distsrc/prefix \
    --build-obj-dir=obj \
    --build-obj-dir-relative \
    --build-config=release \
    --build-link=static \
    --build-skip-tests \
    --build-parallel="${JOBS}" \
    ${OPTIONS}

cd /output
cp "/distsrc/prefix/bin/${PROGRAM}" "${BASENAME}"
"${HOST}-objcopy" --only-keep-debug --compress-debug-sections=zlib "${BASENAME}" "${BASENAME}.debug"
"${HOST}-strip" --strip-all "${BASENAME}"
"${HOST}-objcopy" --add-gnu-debuglink="${BASENAME}.debug" "${BASENAME}"
sha256sum "${BASENAME}" "${BASENAME}.debug" > SHA256SUMS
cat SHA256SUMS
