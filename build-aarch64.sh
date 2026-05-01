#!/bin/bash
# Cross-compile proot for aarch64 Android/Termux
# Prerequisites:
#   - Android NDK (https://developer.android.com/ndk)
#   - Set NDK environment variable or modify NDK_PATH below
#
# Usage:
#   ./build-aarch64.sh
#   Or with custom NDK path:
#   NDK_PATH=/path/to/ndk ./build-aarch64.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
NDK_PATH="${NDK_PATH:-${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}}"
SRC_DIR="${SCRIPT_DIR}/src"
OUTPUT_DIR="${SCRIPT_DIR}/output"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=== Cross-compiling proot for aarch64 Android/Termux ===${NC}"

# Find NDK
if [ -z "$NDK_PATH" ]; then
    echo -e "${YELLOW}Searching for Android NDK...${NC}"
    for path in \
        "$HOME/android-ndk"* \
        "/opt/android-ndk"* \
        "/usr/local/android-ndk"* \
        "$HOME/Android/Sdk/ndk"* \
        "$HOME/Android/Sdk/ndk-bundle"; do
        if [ -d "$path" ]; then
            NDK_PATH="$path"
            break
        fi
    done
fi

if [ -z "$NDK_PATH" ]; then
    echo "Error: Android NDK not found."
    echo ""
    echo "Please install the Android NDK and set NDK_PATH:"
    echo "  1. Download from: https://developer.android.com/ndk/downloads"
    echo "  2. Extract to a directory"
    echo "  3. Run: NDK_PATH=/path/to/ndk $0"
    echo ""
    echo "Alternative: Build directly in Termux:"
    echo "  pkg install proot -y   # or build from source with pkg install make gcc libtalloc-dev"
    exit 1
fi

echo -e "${GREEN}Using NDK: ${NDK_PATH}${NC}"

# Find toolchain
TOOLCHAIN=""
for api in 24 26 28 29 30 31 32 33 34 35; do
    clang_path="${NDK_PATH}/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${api}-clang"
    if [ -x "$clang_path" ]; then
        TOOLCHAIN="${NDK_PATH}/toolchains/llvm/prebuilt/linux-x86_64"
        API_LEVEL="$api"
        break
    fi
done

if [ -z "$TOOLCHAIN" ]; then
    # Try macOS
    for api in 24 26 28 29 30 31 32 33 34 35; do
        clang_path="${NDK_PATH}/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android${api}-clang"
        if [ -x "$clang_path" ]; then
            TOOLCHAIN="${NDK_PATH}/toolchains/llvm/prebuilt/darwin-x86_64"
            API_LEVEL="$api"
            break
        fi
    done
fi

if [ -z "$TOOLCHAIN" ]; then
    echo "Error: Could not find aarch64 clang toolchain in NDK."
    echo "NDK path: ${NDK_PATH}"
    echo "Please ensure you have NDK r21 or later installed."
    exit 1
fi

echo -e "${GREEN}Toolchain: ${TOOLCHAIN} (API ${API_LEVEL})${NC}"

export PATH="${TOOLCHAIN}/bin:${PATH}"
export CC="aarch64-linux-android${API_LEVEL}-clang"
export AR="llvm-ar"
export RANLIB="llvm-ranlib"
export LD="aarch64-linux-android${API_LEVEL}-ld"

# Verify toolchain
echo -e "${YELLOW}Verifying toolchain...${NC}"
"${CC}" --version | head -1

# Build talloc for aarch64
echo ""
echo -e "${YELLOW}Building talloc for aarch64...${NC}"
TALLOC_DIR=$(mktemp -d)
TALLOC_VERSION="2.4.0"
TALLOC_URL="https://www.samba.org/ftp/talloc/talloc-${TALLOC_VERSION}.tar.gz"

cd "${TALLOC_DIR}"
curl -sL "${TALLOC_URL}" -o talloc.tar.gz
tar xzf talloc.tar.gz
cd "talloc-${TALLOC_VERSION}"

# Configure and build talloc
# Create minimal config
cat > config.h << 'CONFIG_EOF'
#ifndef _TALLOC_CONFIG_H
#define _TALLOC_CONFIG_H
#define HAVE_STRNDUP 1
#define HAVE_GETPAGESIZE 1
#define HAVE_SYSCONF 1
#define HAVE_VASPRINTF 1
#define HAVE_ASPRINTF 1
#define HAVE_SECURE_MKSTEMP 1
#define HAVE_SETENV 1
#define HAVE_UNSETENV 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_STRUCT_TIMESPEC 1
#define HAVE_PTHREAD_H 1
#define HAVE___THREAD 1
#define HAVE_GCC_SYNC_LOCK_TEST_AND_SET 1
#define HAVE_ATOMIC_LOCK_FREE 1
#define HAVE_MAYBE_UNUSED_ATTRIBUTE 1
#define HAVE_NONNULL_ATTRIBUTE 1
#define HAVE_RETURNS_NONNULL_ATTRIBUTE 1
#define HAVE_WARN_UNUSED_RESULT_ATTRIBUTE 1
#define HAVE_STDBOOL_H 1
#define HAVE___BOOL 1
#endif
CONFIG_EOF

# Compile talloc
"${CC}" -c talloc.c -o talloc.o \
    -I. -Ilib/replace \
    -D__STDC_WANT_LIB_EXT1__=1 \
    -DHAVE_REPLACE_H \
    -fPIC -O2 \
    -Wno-error

# Create static library
"${AR}" rcs libtalloc.a talloc.o

echo -e "${GREEN}talloc built successfully${NC}"

# Build proot for aarch64
echo ""
echo -e "${YELLOW}Building proot for aarch64...${NC}"
cd "${SRC_DIR}"
make clean 2>/dev/null || true

# Remove --rosegment if not supported (older NDKs)
export CROSS_COMPILE="aarch64-linux-android${API_LEVEL}-"

# Build proot
make \
    CC="${CC}" \
    AR="${AR}" \
    RANLIB="${RANLIB}" \
    CPPFLAGS="-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I. -I${TALLOC_DIR}/talloc-${TALLOC_VERSION} -I${TALLOC_DIR}/talloc-${TALLOC_VERSION}/lib/replace -I${TALLOC_DIR}/talloc-${TALLOC_VERSION}" \
    CFLAGS="-Wall -Wextra -O2 -fPIC" \
    LDFLAGS="-L${TALLOC_DIR}/talloc-${TALLOC_VERSION} -ltalloc -Wl,-z,noexecstack -static" \
    2>&1

if [ -f "proot" ]; then
    mkdir -p "${OUTPUT_DIR}"
    cp proot "${OUTPUT_DIR}/proot-aarch64"
    "${TOOLCHAIN}/bin/llvm-strip" "${OUTPUT_DIR}/proot-aarch64" 2>/dev/null || true
    
    echo ""
    echo -e "${GREEN}=== Build successful! ===${NC}"
    echo ""
    echo "Binary: ${OUTPUT_DIR}/proot-aarch64"
    echo ""
    ls -la "${OUTPUT_DIR}/proot-aarch64"
    echo ""
    echo "Usage in Termux:"
    echo "  ./proot-aarch64 -0 --perm-config /path/to/perms.conf -R /path/to/rootfs /bin/bash"
else
    echo "Error: Build failed."
    exit 1
fi

# Cleanup
rm -rf "${TALLOC_DIR}"

echo ""
echo -e "${GREEN}Done!${NC}"
