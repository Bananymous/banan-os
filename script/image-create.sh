#!/bin/bash

if [ -z $BANAN_DISK_IMAGE_PATH ]; then
	echo  "You must set the BANAN_DISK_IMAGE_PATH environment variable" >&2
	exit 1
fi

if [ -z $BANAN_SYSROOT ]; then
	echo  "You must set the BANAN_SYSROOT environment variable" >&2
	exit 1
fi

if [ -z $BANAN_UEFI_BOOT ]; then
	echo  "You must set the BANAN_UEFI_BOOT environment variable" >&2
	exit 1
fi

if [ -z $BANAN_BUILD_DIR ]; then
	echo  "You must set the BANAN_BUILD_DIR environment variable" >&2
	exit 1
fi

set -u

# create empty disk image
DISK_SIZE=$((4 * 1024 * 1024 * 1024))
truncate -s 0 "$BANAN_DISK_IMAGE_PATH"
truncate -s $DISK_SIZE "$BANAN_DISK_IMAGE_PATH"

# create partition table
if (($BANAN_UEFI_BOOT)); then
	parted --script "$BANAN_DISK_IMAGE_PATH"	\
		mklabel gpt								\
		mkpart boot 1M 17M						\
		set 1 esp on							\
		mkpart root ext2 17M 100%
else
	parted --script "$BANAN_DISK_IMAGE_PATH"	\
		mklabel gpt								\
		mkpart boot 1M 2M						\
		set 1 bios_grub on						\
		mkpart root ext2 2M 100%
fi

root_bounds=($(parted "$BANAN_DISK_IMAGE_PATH" unit B print 2>/dev/null | grep root))
if (( "${#root_bounds[@]}" < 5 )); then
	echo "Unexpected parted output format: ${root_bounds[@]}" >&2
	exit 1
fi

root_blksize=1024
root_offset="${root_bounds[1]::-1}"
root_blocks=$(("${root_bounds[3]::-1}" / "$root_blksize"))

mkfs.ext2 -q \
	-b "$root_blksize" \
	-E offset="$root_offset" \
	"$BANAN_DISK_IMAGE_PATH" \
	"$root_blocks" || exit 1

# install bootloader
$BANAN_SCRIPT_DIR/install-bootloader.sh || exit 1
