#!/usr/bin/env zsh
set -euo pipefail

IMG="test.img"
VOL="TESTDISK"

# Clean previous artifacts
rm -f "$IMG"

# Create a 32 MiB raw image
dd if=/dev/zero of="$IMG" bs=512 count=65536

# Format the file as FAT16 (requires dosfstools from brew)
mkfs.fat -F 16 -n "$VOL" "$IMG"

# Attach raw image as a disk device (no auto-mount)
DEV=$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount "$IMG" \
      | awk 'NR==1 {print $1}')

# Mount the volume
diskutil mount "$DEV" >/dev/null

MNT="/Volumes/$VOL"

# Add test files
echo "hello world" > "$MNT/HELLO.TXT"
echo "readme text" > "$MNT/README.TXT"

mkdir -p "$MNT/SUBDIR/DEEP"

echo "nested file" > "$MNT/SUBDIR/NESTED.TXT"
echo "deep file" > "$MNT/SUBDIR/DEEP/DEEP.TXT"

# Ensure writes are flushed
sync

# Detach disk image
hdiutil detach "$DEV" >/dev/null

echo "Image created: $IMG"
