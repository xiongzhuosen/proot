#!/data/data/com.termux/files/usr/bin/bash
# Build proot for aarch64 directly in Termux
# Run this script in Termux after cloning the repo
#
# Usage in Termux:
#   pkg install make gcc libtalloc-dev
#   cd /path/to/proot
#   chmod +x build-termux.sh
#   ./build-termux.sh

set -e

TERMUX_PREFIX="${TERMUX_PREFIX:-/data/data/com.termux/files/usr}"
SRC_DIR="$(cd "$(dirname "$0")/src" && pwd)"
OUTPUT_DIR="$(cd "$(dirname "$0")" && pwd)/output"

echo "=== Building proot for Termux (aarch64) ==="
echo ""
echo "Checking dependencies..."

# Check for required packages
for cmd in gcc make; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "Error: $cmd not found."
        echo "Run: pkg install $cmd"
        exit 1
    fi
done

# Check for talloc
if [ ! -f "${TERMUX_PREFIX}/include/talloc.h" ]; then
    echo "Error: talloc headers not found."
    echo "Run: pkg install libtalloc-dev"
    exit 1
fi

echo "Dependencies OK."
echo ""

# Build
cd "${SRC_DIR}"
make clean 2>/dev/null || true

echo "Compiling proot..."
make \
    CC=gcc \
    CPPFLAGS="-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I. -I${SRC_DIR}" \
    CFLAGS="-Wall -Wextra -O2" \
    LDFLAGS="-ltalloc -Wl,-z,noexecstack" \
    2>&1

if [ -f "proot" ]; then
    mkdir -p "${OUTPUT_DIR}"
    cp proot "${OUTPUT_DIR}/proot"
    strip "${OUTPUT_DIR}/proot" 2>/dev/null || true
    
    echo ""
    echo "=== Build successful! ==="
    echo ""
    echo "Binary: ${OUTPUT_DIR}/proot"
    ls -la "${OUTPUT_DIR}/proot"
    echo ""
    echo "Test: ${OUTPUT_DIR}/proot --help"
    echo ""
    echo "Usage with permission config:"
    echo "  ${OUTPUT_DIR}/proot -0 --perm-config ./output/example-permissions.conf -R /path/to/rootfs /bin/bash"
else
    echo "Error: Build failed."
    exit 1
fi
