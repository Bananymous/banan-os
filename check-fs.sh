#!/bin/bash

set -e

LOOP_DEV=$(sudo losetup -f --show $DISK_IMAGE_PATH)
sudo partprobe $LOOP_DEV

sudo fsck.ext2 -fn ${LOOP_DEV}p2 || true

sudo losetup -d $LOOP_DEV
