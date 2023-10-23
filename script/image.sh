#!/bin/bash

if [[ -z $BANAN_ROOT_DIR ]]; then
	echo  "You must set the BANAN_ROOT_DIR environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_DISK_IMAGE_PATH ]]; then
	echo  "You must set the BANAN_DISK_IMAGE_PATH environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_SYSROOT ]]; then
	echo  "You must set the BANAN_SYSROOT environment variable" >&2
	exit 1
fi

if [[ ! -f $BANAN_DISK_IMAGE_PATH ]]; then
	$BANAN_SCRIPT_DIR/image-full.sh
	exit 0
fi

fdisk -l $BANAN_DISK_IMAGE_PATH | grep -q 'EFI System'; IMAGE_IS_UEFI=$?
[[ $BANAN_UEFI_BOOT == 1 ]]; CREATE_IS_UEFI=$?

if [[ $IMAGE_IS_UEFI -ne $CREATE_IS_UEFI ]]; then
	echo Converting disk image to/from UEFI
	$BANAN_SCRIPT_DIR/image-full.sh
	exit 0
fi

MOUNT_DIR=/mnt

LOOP_DEV=$(sudo losetup -f --show $BANAN_DISK_IMAGE_PATH)
sudo partprobe $LOOP_DEV

ROOT_PARTITON=${LOOP_DEV}p2

sudo mount $ROOT_PARTITON $MOUNT_DIR

sudo rsync -a ${BANAN_SYSROOT}/* ${MOUNT_DIR}/

sudo umount $MOUNT_DIR

sudo losetup -d $LOOP_DEV
