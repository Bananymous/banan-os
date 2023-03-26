#!/bin/sh
set -e
. ./build.sh

cp -r base/* $SYSROOT

DISK_NAME=banan-os.img
DISK_SIZE=$[50 * 1024 * 1024]
MOUNT_DIR=/mnt

dd if=/dev/zero of=$DISK_NAME bs=512 count=$[$DISK_SIZE / 512]

sed -e 's/\s*\([-\+[:alnum:]]*\).*/\1/' << EOF | fdisk $DISK_NAME
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
  x		# expert menu
  n		# partition name
  3		# ... of partition 3
  mount-test
  n		# partition name
  2		# ... of partition 2
  banan-root
  r		# back to main menu
  w     # write changes
EOF

LOOP_DEV=$(sudo losetup -f --show $DISK_NAME)
sudo partprobe $LOOP_DEV

PARTITION1=${LOOP_DEV}p1
PARTITION2=${LOOP_DEV}p2
PARTITION3=${LOOP_DEV}p3

sudo mkfs.ext2 $PARTITION3
sudo mount $PARTITION3 $MOUNT_DIR
echo 'hello' | sudo tee ${MOUNT_DIR}/hello.txt
sudo umount $MOUNT_DIR

sudo mkfs.ext2 $PARTITION2

sudo mount $PARTITION2 $MOUNT_DIR

sudo cp -r ${SYSROOT}/* ${MOUNT_DIR}/

sudo grub-install --no-floppy --target=i386-pc --modules="normal ext2 multiboot" --boot-directory=${MOUNT_DIR}/boot $LOOP_DEV

echo -e '
menuentry "banan-os" {
	multiboot /boot/banan-os.kernel
}
menuentry "banan-os (no serial)" {
	multiboot /boot/banan-os.kernel noserial
}
menuentry "banan-os (no apic)" {
	multiboot /boot/banan-os.kernel noapic
}
menuentry "banan-os (no apic, no serial)" {
	multiboot /boot/banan-os.kernel noapic noserial
}
' | sudo tee ${MOUNT_DIR}/boot/grub/grub.cfg

sudo umount $MOUNT_DIR

sudo losetup -d $LOOP_DEV
