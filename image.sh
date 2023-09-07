#!/bin/bash
set -e

if [ ! -f $DISK_IMAGE_PATH ]; then
	$(dirname "$0")/image-full.sh
	exit 0
fi

MOUNT_DIR=/mnt

LOOP_DEV=$(sudo losetup -f --show $DISK_IMAGE_PATH)
sudo partprobe $LOOP_DEV

ROOT_PARTITON=${LOOP_DEV}p2

sudo mount $ROOT_PARTITON $MOUNT_DIR

sudo rsync -a ${SYSROOT}/* ${MOUNT_DIR}/

sudo umount $MOUNT_DIR

sudo losetup -d $LOOP_DEV
