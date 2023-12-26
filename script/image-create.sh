#!/bin/bash

if [[ -z $BANAN_DISK_IMAGE_PATH ]]; then
	echo  "You must set the BANAN_DISK_IMAGE_PATH environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_SYSROOT ]]; then
	echo  "You must set the BANAN_SYSROOT environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_TOOLCHAIN_PREFIX ]]; then
	echo  "You must set the BANAN_TOOLCHAIN_PREFIX environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_BOOTLOADER ]]; then
	echo "You must set the BANAN_BOOTLOADER environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_ARCH ]]; then
	echo  "You must set the BANAN_ARCH environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_UEFI_BOOT ]]; then
	echo  "You must set the BANAN_UEFI_BOOT environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_BUILD_DIR ]]; then
	echo  "You must set the BANAN_BUILD_DIR environment variable" >&2
	exit 1
fi

DISK_SIZE=$[500 * 1024 * 1024]
MOUNT_DIR="${MOUNT_DIR:-$BANAN_BUILD_DIR/bananmnt}"

truncate -s 0 "$BANAN_DISK_IMAGE_PATH"
truncate -s $DISK_SIZE "$BANAN_DISK_IMAGE_PATH"

if (($BANAN_UEFI_BOOT)); then
	sed -e 's/\s*\([-\+[:alnum:]]*\).*/\1/' << EOF | fdisk "$BANAN_DISK_IMAGE_PATH" > /dev/null
	  g     # gpt
	  n     # new partition
	  1     # partition number 1
	        # default (from the beginning of the disk)
	  +16M  # efi system size
	  n     # new partition
	  2     # partition number 2
	        # default (right after efi system partition)
			# default (to the end of disk)
	  t     # set type
	  1     # ... of partition 1
	  1     # efi system
	  t     # set type
	  2     # ... of partition 2
	  20    # Linux filesystem
	  w     # write changes
EOF
else
	sed -e 's/\s*\([-\+[:alnum:]]*\).*/\1/' << EOF | fdisk "$BANAN_DISK_IMAGE_PATH" > /dev/null
	  g     # gpt
	  n     # new partition
	  1     # partition number 1
	        # default (from the beginning of the disk)
	  +1M   # bios boot partition size
	  n     # new partition
	  2     # partition number 2
	        # default (right after bios partition)
			# default (to the end of disk)
	  t     # set type
	  1     # ... of partition 1
	  4     # bios boot partition
	  t     # set type
	  2     # ... of partition 2
	  20    # Linux filesystem
	  w     # write changes
EOF
fi

LOOP_DEV=$(sudo losetup -f --show "$BANAN_DISK_IMAGE_PATH")
sudo partprobe $LOOP_DEV

PARTITION1=${LOOP_DEV}p1
PARTITION2=${LOOP_DEV}p2

sudo mkfs.ext2 -q $PARTITION2

sudo mkdir -p $MOUNT_DIR || { echo "Failed to create banan mount dir."; exit 1; }

if [[ "$BANAN_BOOTLOADER" == "GRUB" ]]; then
	if (($BANAN_UEFI_BOOT)); then
		sudo mkfs.fat $PARTITION1 > /dev/null
		sudo mount $PARTITION1 "$MOUNT_DIR"
		sudo mkdir -p "$MOUNT_DIR/EFI/BOOT"
		sudo "$BANAN_TOOLCHAIN_PREFIX/bin/grub-mkstandalone" -O "$BANAN_ARCH-efi" -o "$MOUNT_DIR/EFI/BOOT/BOOTX64.EFI" "boot/grub/grub.cfg=$BANAN_TOOLCHAIN_DIR/grub-memdisk.cfg"
		sudo umount "$MOUNT_DIR"

		sudo mount $PARTITION2 "$MOUNT_DIR"
		sudo mkdir -p "$MOUNT_DIR/boot/grub"
		sudo cp "$BANAN_TOOLCHAIN_DIR/grub-uefi.cfg" "$MOUNT_DIR/boot/grub/grub.cfg"
		sudo umount "$MOUNT_DIR"
	else
		sudo mount $PARTITION2 "$MOUNT_DIR"
		sudo grub-install --no-floppy --target=i386-pc --modules="normal ext2 multiboot" --boot-directory="$MOUNT_DIR/boot" $LOOP_DEV
		sudo mkdir -p "$MOUNT_DIR/boot/grub"
		sudo cp "$BANAN_TOOLCHAIN_DIR/grub-legacy-boot.cfg" "$MOUNT_DIR/boot/grub/grub.cfg"
		sudo umount "$MOUNT_DIR"
	fi
fi

sudo losetup -d $LOOP_DEV || { echo "Failed to remove loop device for banan mount."; exit 1; }

sudo rm -rf $MOUNT_DIR || { echo "Failed to remove banan mount dir."; exit 1; }

if [[ "$BANAN_BOOTLOADER" == "GRUB" ]]; then
	echo > /dev/null
elif [[ "$BANAN_BOOTLOADER" == "BANAN" ]]; then
	if (($BANAN_UEFI_BOOT)); then
		echo "banan bootloader does not support UEFI" >&2
		exit 1
	fi
	$BANAN_SCRIPT_DIR/install-bootloader.sh
else
	echo "unrecognized bootloader $BANAN_BOOTLOADER" >&2
	exit 1
fi
