#!/bin/bash
# CC4060 Firmware Build Script for GitHub Actions
# This script handles the complete build process on Ubuntu

set -e

echo "=== CC4060 BLE Bridge Firmware Builder ==="
echo ""

# Step 1: Download and setup pi32 toolchain
echo "[1/4] Setting up pi32 toolchain..."
TOOLCHAIN_URL="https://github.com/AlexeyGusev/jieli_toolchain/raw/master/pi32v2.tar.gz"
mkdir -p toolchain
cd toolchain

if [ ! -f "pi32v2.tar.gz" ]; then
    echo "Downloading pi32 toolchain..."
    wget -q "$TOOLCHAIN_URL" -O pi32v2.tar.gz || \
    curl -sL "$TOOLCHAIN_URL" -o pi32v2.tar.gz || \
    {
        echo "Error: Failed to download toolchain"
        exit 1
    }
fi

echo "Extracting toolchain..."
tar -xzf pi32v2.tar.gz 2>/dev/null || true
cd ..

# Find the actual toolchain directory
TOOLCHAIN_DIR=$(find toolchain -type d \( -name "pi32v2" -o -name "pi32" -o -name "bin" \) | head -1 | xargs dirname)
if [ -z "$TOOLCHAIN_DIR" ]; then
    TOOLCHAIN_DIR="toolchain"
fi

# Check if compiler exists, if not try alternative paths
COMPILER="$TOOLCHAIN_DIR/bin/pi32-clang"
if [ ! -f "$COMPILER" ]; then
    COMPILER="$TOOLCHAIN_DIR/bin/cc"
fi
if [ ! -f "$COMPILER" ]; then
    # Try extracting from SDK-style archive
    echo "Standard toolchain not found, trying alternative extraction..."
    find toolchain -name "*.tar.*" -exec tar -xf {} \; 2>/dev/null || true
    find toolchain -name "pi32-clang" -o -name "cc" | head -5
fi

echo "Toolchain path: $TOOLCHAIN_DIR"
ls -la "$TOOLCHAIN_DIR/bin/" 2>/dev/null || echo "Warning: bin directory not found"

# Step 2: Verify toolchain
echo ""
echo "[2/4] Verifying toolchain..."
if [ -f "$COMPILER" ]; then
    echo "Compiler found: $COMPILER"
    file "$COMPILER" || true
else
    echo "ERROR: Compiler not found at expected location"
    echo "Available files in toolchain:"
    find toolchain -type f | head -20
    exit 1
fi

# Step 3: Build firmware
echo ""
echo "[3/4] Building firmware..."
make TOOLCHAIN_PATH="$TOOLCHAIN_DIR" || {
    echo "Build failed!"
    exit 1
}

# Step 4: Verify output
echo ""
echo "[4/4] Verifying output..."
if [ -f "updata.bfu" ]; then
    FILESIZE=$(stat -c%s updata.bfu 2>/dev/null || stat -f%z updata.bfu 2>/dev/null)
    echo "✓ Firmware built successfully!"
    echo "  File: updata.bfu"
    echo "  Size: $FILESIZE bytes ($(echo "scale=1; $FILESIZE/1024" | bc) KB)"
    
    # Show BFU info
    python3 tools/bfu_builder.py info updata.bfu
else
    echo "✗ ERROR: updata.bfu not generated!"
    ls -la build/ 2>/dev/null || true
    exit 1
fi

echo ""
echo "=== Build Complete ==="
echo "The updata.bfu file is ready for flashing to your CC4060 device."
