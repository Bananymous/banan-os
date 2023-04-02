#!/bin/sh
set -e

DISK_SIZE=$[50 * 1024 * 1024]
MOUNT_DIR=/mnt

dd if=/dev/zero of=$DISK_IMAGE_PATH bs=512 count=$[$DISK_SIZE / 512] > /dev/null

sed -e 's/\s*\([-\+[:alnum:]]*\).*/\1/' << EOF | fdisk $DISK_IMAGE_PATH > /dev/null
  g     # gpt
  n     # new partition
  1     # partition number 1
        # default (from the beginning of the disk)
  +1MiB # bios boot partiton size
  n		# new partition
  3		# partition number 3
		# default (right after bios boot partition)
  +10Mib# partition size
  n     # new partition
  2     # partition number 2
        # default (right after bios boot partition)
		# default (to the end of disk)
  t     # set type
  1     # ... of partition 1
  4     # bios boot partition
  t     # set type
  2     # ... of partition 2
  20    # Linux filesystem
  t     # set type
  3     # ... of partition 3
  20    # Linux filesystem
  w     # write changes
EOF

LOOP_DEV=$(sudo losetup -f --show $DISK_IMAGE_PATH)
sudo partprobe $LOOP_DEV

PARTITION1=${LOOP_DEV}p1
PARTITION2=${LOOP_DEV}p2
PARTITION3=${LOOP_DEV}p3

sudo mkfs.ext2 $PARTITION3 > /dev/null
sudo mount $PARTITION3 $MOUNT_DIR
echo 'hello' | sudo tee ${MOUNT_DIR}/hello.txt > /dev/null
sudo umount $MOUNT_DIR

sudo mkfs.ext2 $PARTITION2 > /dev/null

sudo mount $PARTITION2 $MOUNT_DIR

sudo cp -r ${SYSROOT}/* ${MOUNT_DIR}/

sudo grub-install --no-floppy --target=i386-pc --modules="normal ext2 multiboot" --boot-directory=${MOUNT_DIR}/boot $LOOP_DEV > /dev/null

sudo umount $MOUNT_DIR

sudo losetup -d $LOOP_DEV
