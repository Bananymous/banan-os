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

# create mount directory
MOUNT_DIR="$BANAN_BUILD_DIR/mount"
mkdir -p $MOUNT_DIR

# create empty disk image
DISK_SIZE=$((500 * 1024 * 1024))
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

# create loop device
LOOP_DEV=$(sudo losetup --show  -fP "$BANAN_DISK_IMAGE_PATH" || exit 1 )
PARTITION1=${LOOP_DEV}p1
PARTITION2=${LOOP_DEV}p2
if [ ! -b $PARTITION1 ] || [ ! -b $PARTITION2 ]; then
	echo "Failed to probe partitions for banan disk image." >&2
	sudo losetup -d $LOOP_DEV
	exit 1
fi

# create root filesystem
sudo mkfs.ext2 -q $PARTITION2

# delete loop device
sudo losetup -d $LOOP_DEV

# install bootloader
$BANAN_SCRIPT_DIR/install-bootloader.sh
