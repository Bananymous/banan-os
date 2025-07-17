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

mount_dir="$BANAN_BUILD_DIR/mount"
mkdir -p $mount_dir

install_grub_legacy() {
	sudo mount $partition2 "$mount_dir"
	sudo grub-install						\
		--no-floppy							\
		--target=i386-pc					\
		--modules="normal ext2 multiboot"	\
		--boot-directory="$mount_dir/boot"	\
		$loop_dev
	sudo mkdir -p "$mount_dir/boot/grub"
	if (($BANAN_INITRD)); then
		sudo cp "$BANAN_BUILD_DIR/grub-legacy-initrd.cfg" "$mount_dir/boot/grub/grub.cfg"
	else
		sudo cp "$BANAN_BUILD_DIR/grub-legacy.cfg" "$mount_dir/boot/grub/grub.cfg"
	fi
	sudo umount "$mount_dir"
}

install_grub_uefi() {
	sudo mkfs.fat $partition1 > /dev/null
	sudo mount $partition1 "$mount_dir"
	sudo mkdir -p "$mount_dir/EFI/BOOT"
	sudo "$BANAN_TOOLCHAIN_PREFIX/bin/grub-mkstandalone" -O "$BANAN_ARCH-efi" -o "$mount_dir/EFI/BOOT/BOOTX64.EFI" "boot/grub/grub.cfg=$BANAN_BUILD_DIR/grub-memdisk.cfg"
	sudo umount "$mount_dir"

	sudo mount $partition2 "$mount_dir"
	sudo mkdir -p "$mount_dir/boot/grub"
	if (($BANAN_INITRD)); then
		sudo cp "$BANAN_BUILD_DIR/grub-uefi-initrd.cfg" "$mount_dir/boot/grub/grub.cfg"
	else
		sudo cp "$BANAN_BUILD_DIR/grub-uefi.cfg" "$mount_dir/boot/grub/grub.cfg"
	fi
	sudo umount "$mount_dir"
}

install_banan_legacy() {
	if (($BANAN_INITRD)); then
		echo "banan bootloader does not support initrd" >&2
		exit 1
	fi

	root_disk_info=$(fdisk -x "$BANAN_DISK_IMAGE_PATH" | tr -s ' ')
	root_part_guid=$(echo "$root_disk_info" | grep "^$BANAN_DISK_IMAGE_PATH" | head -2 | tail -1 | cut -d' ' -f6)

	installer_build_dir=$BANAN_ROOT_DIR/bootloader/installer/build/$BANAN_ARCH
	bootloader_elf=$BANAN_BUILD_DIR/bootloader/bios/bootloader

	if [ ! -f $bootloader_elf ]; then
		echo "You must build the bootloader first" >&2
		exit 1
	fi

	if [ ! -d $installer_build_dir ]; then
		mkdir -p $installer_build_dir
		cd $installer_build_dir
		$BANAN_CMAKE -G Ninja ../..
	fi

	cd $installer_build_dir
	ninja

	echo installing bootloader
	$installer_build_dir/banan_os-bootloader-installer "$bootloader_elf" "$BANAN_DISK_IMAGE_PATH" "$root_part_guid"
}

install_banan_uefi() {
	echo "UEFI boot is not supported by the BANAN bootloader" >&2
	exit 1
}

if [ $BANAN_BOOTLOADER = "GRUB" ]; then
	loop_dev=$(sudo losetup --show -fP $BANAN_DISK_IMAGE_PATH || exit 1)
	partition1=${loop_dev}p1
	partition2=${loop_dev}p2
	if [ ! -b $partition1 ] || [ ! -b $partition2 ]; then
		echo "Failed to probe partitions for banan disk image." >&2
		sudo losetup -d $loop_dev
		exit 1
	fi

	root_part_info=$(sudo blkid $partition2)

	root_fs_uuid=$(grep -Pwo 'UUID=".*?"' <<< "$root_part_info")
	root_fs_uuid=${root_fs_uuid:6:36}

	root_part_uuid=$(grep -Pwo 'PARTUUID=".*?"' <<< "$root_part_info")
	root_part_uuid=${root_part_uuid:10:36}

	cp "$BANAN_TOOLCHAIN_DIR"/grub-*.cfg "$BANAN_BUILD_DIR/"
	sed -i "s/<ROOT>/PARTUUID=$root_part_uuid/" "$BANAN_BUILD_DIR"/grub-*.cfg
	sed -i "s/<ROOT_FS>/$root_fs_uuid/" "$BANAN_BUILD_DIR"/grub-*.cfg

	if (($BANAN_UEFI_BOOT)); then
		install_grub_uefi
	else
		install_grub_legacy
	fi

	sudo losetup -d $loop_dev

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
