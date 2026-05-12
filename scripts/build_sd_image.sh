#!/usr/bin/env bash
# build_sd_image.sh — regenerate the DE1-SoC SD card image.
#
# Run this after a Quartus build that includes the mcts_accel component.
# Produces a bootable sdcard.img under hw/output_files/.
#
# Requires: Quartus 21.1 (qsys-script, quartus_sh), SoC EDS (bsp-create-settings,
# bsp-update-files), arm-altera-eabi-gcc, dtc, mkimage, parted, sudo, dd.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HW_DIR="$REPO_ROOT/hw"
OUT="$HW_DIR/output_files"
IMG="$OUT/sdcard.img"
PARTITION_SIZE_MB=512

cd "$HW_DIR"

echo "== 1. Regenerate Qsys =="
qsys-generate soc_system.qsys --synthesis=VERILOG

echo "== 2. Quartus full compile =="
quartus_sh --flow compile soc_system

echo "== 3. Convert .sof to .rbf =="
quartus_cpf -c -o bitstream_compression=on \
    "$OUT/soc_system.sof" "$OUT/soc_system.rbf"

echo "== 4. Preloader (SPL) =="
bsp-create-settings --bsp-dir software/spl_bsp --type spl \
    --settings software/spl_bsp/settings.bsp \
    --preset 'Intel SoCFPGA - HPS BSP' \
    --preloader-settings-dir hps_isw_handoff/soc_system_hps_0
make -C software/spl_bsp

echo "== 5. Device tree =="
if [ -f soc_system.dts ]; then
    dtc -I dts -O dtb -o "$OUT/soc_system.dtb" soc_system.dts
fi

echo "== 6. Assemble image =="
# This step depends heavily on your local Linux distro for the DE1-SoC. The
# script assumes the standard 3-partition layout: A2 (preloader), VFAT
# (kernel + dtb + rbf), ext3 (rootfs).
#
# Practical guidance:
#   - Mount existing sdcard.img → write fresh soc_system.rbf and dtb to VFAT
#   - Re-write A2 partition with the preloader
# This script is left as a skeleton for the user to fill in for their setup.

if [ ! -f "$IMG" ]; then
    echo "ERROR: $IMG not present. Build it from a base Linux image once,"
    echo "       then this script can keep refreshing it."
    exit 1
fi

# Mount VFAT partition (assumed to be #1) and copy new rbf/dtb
MNT=$(mktemp -d)
LOOP=$(sudo losetup -fP --show "$IMG")
trap 'sudo umount "$MNT" 2>/dev/null || true; sudo losetup -d "$LOOP" || true; rmdir "$MNT"' EXIT
sudo mount "${LOOP}p1" "$MNT"
sudo cp "$OUT/soc_system.rbf" "$MNT/soc_system.rbf"
[ -f "$OUT/soc_system.dtb" ] && sudo cp "$OUT/soc_system.dtb" "$MNT/soc_system.dtb"
sudo umount "$MNT"
sudo losetup -d "$LOOP"
trap - EXIT
rmdir "$MNT"

echo "== Done: $IMG =="
