#!/bin/bash

if [ -z $BANAN_ARCH ]; then
	echo "You must set the BANAN_ARCH environment variable" >&2
	exit 1
fi

if [ -z $BANAN_DISK_IMAGE_PATH ]; then
	echo "You must set the BANAN_DISK_IMAGE_PATH environment variable" >&2
	exit 1
fi

if [ -z $BANAN_BUILD_DIR ]; then
	echo "You must set the BANAN_BUILD_DIR environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_UEFI_BOOT ]]; then
	echo  "You must set the BANAN_UEFI_BOOT environment variable" >&2
	exit 1
fi

if [ -z $BANAN_ROOT_DIR ]; then
	echo "You must set the BANAN_ROOT_DIR environment variable" >&2
	exit 1
fi

if [ -z $BANAN_CMAKE ]; then
	echo "You must set the BANAN_CMAKE environment variable" >&2
	exit 1
fi

set -u

MOUNT_DIR="$BANAN_BUILD_DIR/mount"
mkdir -p $MOUNT_DIR

install_grub_legacy() {
	sudo mount $PARTITION2 "$MOUNT_DIR"
	sudo grub-install						\
		--no-floppy							\
		--target=i386-pc					\
		--modules="normal ext2 multiboot"	\
		--boot-directory="$MOUNT_DIR/boot"	\
		$LOOP_DEV
	sudo mkdir -p "$MOUNT_DIR/boot/grub"
	sudo cp "$BANAN_TOOLCHAIN_DIR/grub-legacy-boot.cfg" "$MOUNT_DIR/boot/grub/grub.cfg"
	sudo umount "$MOUNT_DIR"
}

install_grub_uefi() {
	sudo mkfs.fat $PARTITION1 > /dev/null
	sudo mount $PARTITION1 "$MOUNT_DIR"
	sudo mkdir -p "$MOUNT_DIR/EFI/BOOT"
	sudo "$BANAN_TOOLCHAIN_PREFIX/bin/grub-mkstandalone" -O "$BANAN_ARCH-efi" -o "$MOUNT_DIR/EFI/BOOT/BOOTX64.EFI" "boot/grub/grub.cfg=$BANAN_TOOLCHAIN_DIR/grub-memdisk.cfg"
	sudo umount "$MOUNT_DIR"

	sudo mount $PARTITION2 "$MOUNT_DIR"
	sudo mkdir -p "$MOUNT_DIR/boot/grub"
	sudo cp "$BANAN_TOOLCHAIN_DIR/grub-uefi.cfg" "$MOUNT_DIR/boot/grub/grub.cfg"
	sudo umount "$MOUNT_DIR"
}

install_banan_legacy() {
	ROOT_PARTITION_INDEX=2
	ROOT_PARTITION_INFO=$(fdisk -x $BANAN_DISK_IMAGE_PATH | grep "^$BANAN_DISK_IMAGE_PATH" | head -$ROOT_PARTITION_INDEX | tail -1)
	ROOT_PARTITION_GUID=$(echo $ROOT_PARTITION_INFO | cut -d' ' -f6)

	INSTALLER_BUILD_DIR=$BANAN_ROOT_DIR/bootloader/installer/build/$BANAN_ARCH
	BOOTLOADER_ELF=$BANAN_BUILD_DIR/bootloader/bios/bootloader

	if [ ! -f $BOOTLOADER_ELF ]; then
		echo "You must build the bootloader first" >&2
		exit 1
	fi

	if [ ! -d $INSTALLER_BUILD_DIR ]; then
		mkdir -p $INSTALLER_BUILD_DIR
		cd $INSTALLER_BUILD_DIR
		$BANAN_CMAKE -G Ninja ../..
	fi

	cd $INSTALLER_BUILD_DIR
	ninja

	echo installing bootloader
	$INSTALLER_BUILD_DIR/banan_os-bootloader-installer $BOOTLOADER_ELF $BANAN_DISK_IMAGE_PATH $ROOT_PARTITION_GUID
}

install_banan_uefi() {
	echo "UEFI boot is not supported by the BANAN bootloader" >&2
	exit 1
}

if [ $BANAN_BOOTLOADER = "GRUB" ]; then

	LOOP_DEV=$(sudo losetup --show -fP $BANAN_DISK_IMAGE_PATH || exit 1)
	PARTITION1=${LOOP_DEV}p1
	PARTITION2=${LOOP_DEV}p2
	if [ ! -b $PARTITION1 ] || [ ! -b $PARTITION2 ]; then
		echo "Failed to probe partitions for banan disk image." >&2
		sudo losetup -d $LOOP_DEV
		exit 1
	fi

	if (($BANAN_UEFI_BOOT)); then
		install_grub_uefi
	else
		install_grub_legacy
	fi

	sudo losetup -d $LOOP_DEV

elif [ $BANAN_BOOTLOADER = "BANAN" ]; then
	if (($BANAN_UEFI_BOOT)); then
		install_banan_uefi
	else
		install_banan_legacy
	fi
else
	echo "Unknown bootloader $BANAN_BOOTLOADER" >&2
	exit 1
fi
