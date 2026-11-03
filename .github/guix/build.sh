#!/bin/bash
###############################################################################
#  Copyright (c) 2014-2026 libbitcoin-server developers (see COPYING).
#  store_path, the cross search paths and the darwin compiler flags derive
#  from Bitcoin Core contrib/guix/libexec and depends/hosts/darwin.mk,
#  Copyright (c) The Bitcoin Core developers, MIT license
#  (https://opensource.org/license/mit).
###############################################################################
# Build one release binary inside the Guix container.
# environment: HOST, BASENAME, PROJECT, PROGRAM, FLAGS, LINKFLAGS, OPTIONS, JOBS
# darwin also: DEPLOYMENT_TARGET, with the Apple SDK exposed at /sdk
# windows also: the MSVC and Windows SDK exposed at /winsdk
set -euo pipefail

export LC_ALL=C.UTF-8
export TZ=UTC
export HOME=/tmp
umask 0022

EPOCH=946684800
STORE_MAP="$(find /gnu/store -maxdepth 1 -mindepth 1 -type d -exec echo -n " -ffile-prefix-map={}=/usr" \;)"
DEBUG_FLAGS="-g -ffile-prefix-map=/distsrc=. -gno-record-gcc-switches${STORE_MAP}"

store_path()
{
    grep --extended-regexp "/[^-]{32}-${1}-[^-]+${2:+-${2}}" "${GUIX_ENVIRONMENT}/manifest" \
        | head --lines=1 \
        | sed --expression='s|\x29*$||' \
              --expression='s|^[[:space:]]*"||' \
              --expression='s|"[[:space:]]*$||'
}

# Linux
# -----------------------------------------------------------------------------

configure_linux()
{
    local LOADER
    case "${HOST}" in
        x86_64-linux-gnu)  LOADER=/lib64/ld-linux-x86-64.so.2 ;;
        aarch64-linux-gnu) LOADER=/lib/ld-linux-aarch64.so.1 ;;
        *) echo "Unsupported host '${HOST}'." >&2; exit 1 ;;
    esac

    local GLIBC="$(store_path "glibc-cross-${HOST}")"
    local GLIBC_STATIC="$(store_path "glibc-cross-${HOST}" static)"
    local KERNEL="$(store_path "linux-libre-headers-cross-${HOST}")"
    local GCC="$(store_path "gcc-cross-${HOST}")"
    local GCC_LIB_STORE="$(store_path "gcc-cross-${HOST}" lib)"
    local GCC_LIBS=( "${GCC_LIB_STORE}/lib/gcc/${HOST}"/* )
    local GCC_LIB="${GCC_LIBS[0]}"

    export CROSS_C_INCLUDE_PATH="${GCC_LIB}/include:${GCC_LIB}/include-fixed:${GLIBC}/include:${KERNEL}/include"
    export CROSS_CPLUS_INCLUDE_PATH="${GCC}/include/c++:${GCC}/include/c++/${HOST}:${GCC}/include/c++/backward:${CROSS_C_INCLUDE_PATH}"
    export CROSS_LIBRARY_PATH="${GCC_LIB_STORE}/lib:${GCC_LIB}:${GLIBC}/lib:${GLIBC_STATIC}/lib"
    export GUIX_LD_WRAPPER_DISABLE_RPATH=yes

    export CC="${HOST}-gcc"
    export CXX="${HOST}-g++"
    export CFLAGS="${FLAGS} ${DEBUG_FLAGS}"
    export CXXFLAGS="${CFLAGS}"
    export LDFLAGS="${LINKFLAGS} -Wl,--build-id -Wl,--dynamic-linker=${LOADER}"

    SYSTEM=(
        -DCMAKE_SYSTEM_NAME=Linux
        -DCMAKE_SYSTEM_PROCESSOR="${HOST%%-*}"
        -DCMAKE_C_STANDARD_LIBRARIES=-Wl,-lpthread,-ldl
        -DCMAKE_CXX_STANDARD_LIBRARIES=-Wl,-lpthread,-ldl )
}

package_linux()
{
    BINARY="${BASENAME}"
    SYMBOLS="${BASENAME}.debug"
    cp "/distsrc/prefix/bin/${PROGRAM}" "${BINARY}"
    "${HOST}-objcopy" --only-keep-debug --compress-debug-sections=zlib "${BINARY}" "${SYMBOLS}"
    "${HOST}-strip" --strip-all "${BINARY}"
    "${HOST}-objcopy" --add-gnu-debuglink="${SYMBOLS}" "${BINARY}"
}

# macOS
# -----------------------------------------------------------------------------

configure_darwin()
{
    unset C_INCLUDE_PATH CPLUS_INCLUDE_PATH OBJC_INCLUDE_PATH OBJCPLUS_INCLUDE_PATH CPATH LIBRARY_PATH

    local SDK_FLAGS="--target=${HOST} -mmacos-version-min=${DEPLOYMENT_TARGET} -mlinker-version=711 -isysroot/sdk -nostdlibinc"
    local SDK_INCLUDES="-iwithsysroot/usr/include -iframeworkwithsysroot/System/Library/Frameworks"

    export CC=clang
    export CXX=clang++
    export CFLAGS="${SDK_FLAGS} ${SDK_INCLUDES} ${FLAGS} ${DEBUG_FLAGS}"
    export CXXFLAGS="${SDK_FLAGS} -iwithsysroot/usr/include/c++/v1 ${SDK_INCLUDES} -Xclang -fno-cxx-modules ${FLAGS} ${DEBUG_FLAGS}"
    export LDFLAGS="${SDK_FLAGS} ${LINKFLAGS} -fuse-ld=lld -Wl,-oso_prefix,/distsrc/"

    SYSTEM=(
        -DCMAKE_SYSTEM_NAME=Darwin
        -DCMAKE_SYSTEM_PROCESSOR="${HOST%%-*}"
        -DCMAKE_AR="$(command -v llvm-ar)"
        -DCMAKE_RANLIB="$(command -v llvm-ranlib)"
        -DCMAKE_INSTALL_NAME_TOOL="$(command -v llvm-install-name-tool)" )
}

package_darwin()
{
    BINARY="${BASENAME}"
    SYMBOLS="${BASENAME}.dSYM.zip"
    cp "/distsrc/prefix/bin/${PROGRAM}" "${BINARY}"
    dsymutil --oso-prepend-path=/distsrc "${BINARY}" -o "${BASENAME}.dSYM"
    llvm-strip -x "${BINARY}"
    find "${BASENAME}.dSYM" -exec touch --no-dereference --date="@${EPOCH}" {} +
    find "${BASENAME}.dSYM" | sort | zip -X -q "${SYMBOLS}" -@
    rm -rf "${BASENAME}.dSYM"
}

# Windows
# -----------------------------------------------------------------------------

# The SDK is spelled for a case-insensitive filesystem. Compiler and linker
# read it through a case-insensitive overlay; the resource preprocessor gets
# links for each include whose spelling differs from the file it names.
case_insensitive_sdk()
{
    OVERLAY=/tmp/winsdk.yaml
    {
        echo "version: 0"
        echo "case-sensitive: false"
        echo "roots:"
        find /winsdk -type f | sort | sed "s|.*|  - { name: '&', type: file, external-contents: '&' }|"
    } > "${OVERLAY}"

    CASED=/tmp/cased
    mkdir -p "${CASED}"
    find "${SDK_INCLUDE}" -type f | awk -F/ '{ print tolower($NF) "\t" $0 }' | sort > /tmp/headers
    grep -rhoE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"][^<>"/]+[>"]' "${SDK_INCLUDE}" \
        | sed -E 's|.*[<"]([^<>"]+)[>"]|\1|' | sort -u \
        | awk '{ print tolower($0) "\t" $0 }' | sort > /tmp/includes
    join -t "$(printf '\t')" /tmp/includes /tmp/headers | while IFS="$(printf '\t')" read -r _ NAME FILE; do
        [[ "$(basename "${FILE}")" == "${NAME}" ]] || ln -sf "${FILE}" "${CASED}/${NAME}"
    done
}

configure_windows()
{
    unset C_INCLUDE_PATH CPLUS_INCLUDE_PATH CPATH LIBRARY_PATH

    local ARCH PROCESSOR
    case "${HOST}" in
        x86_64-pc-windows-msvc)  ARCH=x64;   PROCESSOR=AMD64 ;;
        aarch64-pc-windows-msvc) ARCH=arm64; PROCESSOR=ARM64 ;;
        *) echo "Unsupported host '${HOST}'." >&2; exit 1 ;;
    esac

    local SDK_VERSION="$(ls /winsdk/sdk/Include)"
    local SDK_LIB="/winsdk/sdk/Lib/${SDK_VERSION}"
    SDK_INCLUDE="/winsdk/sdk/Include/${SDK_VERSION}"
    case_insensitive_sdk

    local CL_MAP="$(echo " -ffile-prefix-map=/distsrc=.${STORE_MAP}" | sed 's| -| -clang:-|g')"

    export CC=clang-cl
    export CXX=clang-cl
    export CFLAGS="--target=${HOST} -vfsoverlay ${OVERLAY} -imsvc /winsdk/msvc/include -imsvc ${SDK_INCLUDE}/ucrt -imsvc ${SDK_INCLUDE}/um -imsvc ${SDK_INCLUDE}/shared -Brepro ${FLAGS} ${CL_MAP}"
    export CXXFLAGS="${CFLAGS}"
    export RCFLAGS="-I ${SDK_INCLUDE}/um -I ${SDK_INCLUDE}/shared -I ${CASED}"
    export LDFLAGS="-vfsoverlay:${OVERLAY} -libpath:/winsdk/msvc/lib/${ARCH} -libpath:${SDK_LIB}/ucrt/${ARCH} -libpath:${SDK_LIB}/um/${ARCH} -Brepro -debug:full -pdbaltpath:%_PDB% /MANIFEST:NO ${LINKFLAGS}"

    SYSTEM=(
        -DCMAKE_SYSTEM_NAME=Windows
        -DCMAKE_SYSTEM_VERSION=10.0
        -DCMAKE_SYSTEM_PROCESSOR="${PROCESSOR}"
        -DCMAKE_LINKER="$(command -v lld-link)"
        -DCMAKE_AR="$(command -v llvm-lib)"
        -DCMAKE_MT="$(command -v llvm-mt)"
        -DCMAKE_RC_COMPILER="$(command -v llvm-rc)"
        -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
        -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded )
}

package_windows()
{
    BINARY="${BASENAME}.exe"
    SYMBOLS="${BASENAME}.pdb.zip"
    cp "/distsrc/prefix/bin/${PROGRAM}.exe" "${BINARY}"
    cp "$(find /distsrc -path "*/obj/*" -name "${PROGRAM}.pdb" | head -1)" "${PROGRAM}.pdb"
    touch --date="@${EPOCH}" "${PROGRAM}.pdb"
    zip -X -q "${SYMBOLS}" "${PROGRAM}.pdb"
    rm "${PROGRAM}.pdb"
}

# Build
# -----------------------------------------------------------------------------

case "${HOST}" in
    *-linux-gnu)       PLATFORM=linux ;;
    *-apple-darwin)    PLATFORM=darwin ;;
    *-pc-windows-msvc) PLATFORM=windows ;;
    *) echo "Unsupported host '${HOST}'." >&2; exit 1 ;;
esac

"configure_${PLATFORM}"

bash "/distsrc/${PROJECT}/builds/cmake/install-cmake.sh" \
    "${SYSTEM[@]}" \
    -DCMAKE_SKIP_INSTALL_RPATH=ON \
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
"package_${PLATFORM}"
sha256sum "${BINARY}" "${SYMBOLS}" > SHA256SUMS
cat SHA256SUMS
