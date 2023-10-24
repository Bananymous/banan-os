#!/bin/bash

if [[ -z $BANAN_DISK_IMAGE_PATH ]]; then
	echo "You must set BANAN_DISK_IMAGE_PATH environment variable" >&2
	exit 1
fi

LOOP_DEV=$(sudo losetup -f --show $BANAN_DISK_IMAGE_PATH)
sudo partprobe $LOOP_DEV

sudo fsck.ext2 -fn ${LOOP_DEV}p2 || true

sudo losetup -d $LOOP_DEV
