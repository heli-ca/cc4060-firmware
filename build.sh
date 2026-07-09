#!/bin/bash
# build.sh - CC4060 Firmware Build Script
# Usage:
#   build.sh setup    - Extract SDK and toolchain, apply patches
#   build.sh compile  - Run make
#   build.sh post_build - Post-build: objcopy + firmware image + BFU
#   build.sh all      - All of the above

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_ROOT="${SDK_ROOT:-$SCRIPT_DIR}"
TOOLCHAIN_TAR="${SDK_ROOT}/pi32v2_toolchain.tar.gz"
SDK_TAR="${SDK_ROOT}/sdk_ac692x.tar.gz"
PATCH_SCRIPT="${SCRIPT_DIR}/tools/cc4060_patch.py"
OUTPUT_DIR="${SDK_ROOT}/apps/download/post_build/flash"

echo "=========================================="
echo " CC4060 Firmware Build"
echo " SDK_ROOT: ${SDK_ROOT}"
echo "=========================================="

# ---- Setup: Extract toolchain and SDK ----
setup() {
    echo ""
    echo "===== STEP 1: Setup Toolchain ====="

    # Install toolchain to /opt/pi32/upstream
    sudo mkdir -p /opt/pi32/upstream
    if [ -f "$TOOLCHAIN_TAR" ]; then
        sudo tar xzf "$TOOLCHAIN_TAR" -C /opt/pi32/upstream
        echo "Toolchain extracted to /opt/pi32/upstream"
    else
        echo "ERROR: Toolchain tarball not found at $TOOLCHAIN_TAR"
        exit 1
    fi

    # Verify toolchain
    ls -la /opt/pi32/upstream/bin/
    file /opt/pi32/upstream/bin/clang || true

    echo ""
    echo "===== STEP 2: Extract SDK Source ====="

    if [ -f "$SDK_TAR" ]; then
        tar xzf "$SDK_TAR" -C "${SDK_ROOT}"
        echo "SDK source extracted"
    else
        echo "WARNING: SDK tarball not found, assuming already extracted"
    fi

    echo ""
    echo "===== STEP 3: Install Build Rules ====="

    if [ -f "${SCRIPT_DIR}/tools/rule.mk" ]; then
        cp "${SCRIPT_DIR}/tools/rule.mk" "${SDK_ROOT}/tools/rule.mk"
        echo "rule.mk installed"
    else
        echo "ERROR: rule.mk not found at ${SCRIPT_DIR}/tools/rule.mk"
        exit 1
    fi

    echo ""
    echo "===== STEP 4: Apply CC4060 Patch ====="

    python3 "$PATCH_SCRIPT" "$SDK_ROOT"

    echo ""
    echo "===== SETUP COMPLETE ====="
}

# ---- Compile ----
compile() {
    echo ""
    echo "===== STEP 5: Compile ====="

    cd "${SDK_ROOT}"
    make HOST_OS=linux SLASH=/ \
        -f tools/platform/Makefile.br21 \
        2>&1 | tail -100

    echo ""
    echo "===== Compilation done ====="
    ls -la "${OUTPUT_DIR}/sdk.exe" 2>/dev/null || echo "WARNING: sdk.exe not found"
}

# ---- Post-build: objcopy + isd_download + bfumake ----
post_build() {
    echo ""
    echo "===== STEP 6: Post-Build (objcopy) ====="

    cd "${OUTPUT_DIR}"

    if [ ! -f sdk.exe ]; then
        echo "ERROR: sdk.exe not found in ${OUTPUT_DIR}"
        ls -la
        exit 1
    fi

    OBJCOPY=/opt/pi32/upstream/bin/objcopy

    # Extract sections
    $OBJCOPY -O binary -j .text  sdk.exe sdk.bin
    $OBJCOPY -O binary -j .data  sdk.exe data.bin
    $OBJCOPY -O binary -j .nvdata sdk.exe nvdata.bin

    # Concatenate into sdk.app
    cat sdk.bin data.bin nvdata.bin > sdk.app
    echo "sdk.app created: $(ls -la sdk.app)"

    echo ""
    echo "===== STEP 7: Firmware Image (isd_download via Wine) ====="

    # Install Wine if needed
    if ! command -v wine >/dev/null 2>&1; then
        echo "Installing Wine..."
        sudo apt-get update -qq
        sudo dpkg --add-architecture i386
        sudo apt-get install -y -qq wine wine32 wine64 2>/dev/null || \
        sudo apt-get install -y -qq wine64 2>/dev/null || true
    fi

    # Check for isd_download.exe
    ISD_EXE="${OUTPUT_DIR}/tool_resource/dev_ota/isd_download.exe"
    if [ ! -f "$ISD_EXE" ]; then
        ISD_EXE=$(find "${SDK_ROOT}" -name "isd_download.exe" 2>/dev/null | head -1)
    fi

    if [ -f "$ISD_EXE" ]; then
        echo "Using isd_download.exe: $ISD_EXE"

        # Build resource file list
        RESOURCES=""
        for f in bt.mp3 music.mp3 linein.mp3 radio.mp3 connect.mp3 disconnect.mp3 \
                 ring.mp3 warning.mp3 power_off.mp3 0.mp3 1.mp3 2.mp3 3.mp3 4.mp3 \
                 5.mp3 6.mp3 7.mp3 8.mp3 9.mp3 start.mp3; do
            if [ -f "${OUTPUT_DIR}/$f" ]; then
                RESOURCES="$RESOURCES $f"
            fi
        done

        # Run isd_download
        cd "${OUTPUT_DIR}"
        wine "$ISD_EXE" -tonorflash -dev br21 -boot 0x2000 -div6 -wait 300 \
            -format cfg -f uboot.boot sdk.app bt_cfg.bin $RESOURCES 2>&1 || {
            echo "WARNING: isd_download failed via Wine"
            ls -la
        }

        if [ -f jl_isd.bin ]; then
            echo "jl_isd.bin created: $(ls -la jl_isd.bin)"
        else
            echo "ERROR: jl_isd.bin not generated"
            ls -la
            exit 1
        fi
    else
        echo "ERROR: isd_download.exe not found!"
        echo "Searched in: ${OUTPUT_DIR}/tool_resource/dev_ota/"
        find "${SDK_ROOT}" -name "isd_download.exe" 2>/dev/null || true
        exit 1
    fi

    echo ""
    echo "===== STEP 8: BFU Packaging ====="

    cd "${OUTPUT_DIR}"

    # Use Python bfu_builder.py
    BFU_BUILDER="${SCRIPT_DIR}/tools/bfu_builder.py"
    if [ -f "$BFU_BUILDER" ]; then
        python3 "$BFU_BUILDER" build jl_isd.bin updata.bfu
        echo "updata.bfu created: $(ls -la updata.bfu)"
    else
        echo "WARNING: bfu_builder.py not found at $BFU_BUILDER"
        echo "Trying bfumake.exe via Wine..."
        BFUMAKE="${OUTPUT_DIR}/tool_resource/dev_ota/bfumake.exe"
        if [ -f "$BFUMAKE" ]; then
            wine "$BFUMAKE" -fi jl_isd.bin -ld 0x0000 -rd 0x0000 -fo updata.bfu
            echo "updata.bfu created via bfumake: $(ls -la updata.bfu)"
        else
            echo "ERROR: Neither bfu_builder.py nor bfumake.exe found"
            exit 1
        fi
    fi

    echo ""
    echo "=========================================="
    echo " BUILD SUCCESSFUL!"
    echo " Firmware: ${OUTPUT_DIR}/updata.bfu"
    echo "=========================================="
    ls -la "${OUTPUT_DIR}/updata.bfu"
}

# ---- Main ----
case "${1:-all}" in
    setup)
        setup
        ;;
    compile)
        compile
        ;;
    post_build)
        post_build
        ;;
    all)
        setup
        compile
        post_build
        ;;
    *)
        echo "Usage: $0 {setup|compile|post_build|all}"
        exit 1
        ;;
esac
