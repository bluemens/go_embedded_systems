#!/usr/bin/env bash
# flash_sd.sh — write hw/output_files/sdcard.img to the SD card.
#
# Safety: refuses to run unless SD_DEV is exported and points at a
# removable device. Always confirm with `lsblk`/`diskutil list` first.
#
# Usage:
#   SD_DEV=/dev/disk4 ./scripts/flash_sd.sh        # macOS (raw disk)
#   SD_DEV=/dev/sdc   ./scripts/flash_sd.sh        # Linux

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="$REPO_ROOT/hw/output_files/sdcard.img"

if [ -z "${SD_DEV:-}" ]; then
    echo "ERROR: SD_DEV not set. Export it (e.g., SD_DEV=/dev/disk4) and rerun." >&2
    exit 2
fi

if [ ! -f "$IMG" ]; then
    echo "ERROR: image not present: $IMG" >&2
    exit 2
fi

case "$SD_DEV" in
    /dev/disk[0-9]*|/dev/rdisk[0-9]*|/dev/sd[a-z]|/dev/mmcblk[0-9]*) ;;
    *) echo "ERROR: SD_DEV ($SD_DEV) does not look like an SD card device." >&2
       exit 2 ;;
esac

echo "About to dd $IMG → $SD_DEV"
read -r -p "Type 'yes' to continue: " ack
[ "$ack" = "yes" ] || { echo "Aborted."; exit 1; }

case "$(uname)" in
    Darwin)  diskutil unmountDisk "$SD_DEV" || true
             sudo dd if="$IMG" of="${SD_DEV/disk/rdisk}" bs=1m status=progress ;;
    Linux)   sudo umount "${SD_DEV}"* 2>/dev/null || true
             sudo dd if="$IMG" of="$SD_DEV" bs=1M status=progress oflag=sync ;;
esac
sync
echo "Done. Eject and reseat into DE1-SoC."
