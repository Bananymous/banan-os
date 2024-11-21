#!/bin/bash

if [[ -z $BANAN_DISK_IMAGE_PATH ]]; then
	echo "You must set the BANAN_DISK_IMAGE_PATH environment variable" >&2
	exit 1
fi

if [[ -z $OVMF_PATH ]]; then
	OVMF_PATH="/usr/share/ovmf/x64/OVMF.fd"
fi

if (($BANAN_UEFI_BOOT)); then
	BIOS_ARGS="-bios $OVMF_PATH -net none"
fi

if [[ $BANAN_DISK_TYPE == "NVME" ]]; then
	DISK_ARGS="-device nvme,serial=deadbeef,drive=disk"
elif [[ $BANAN_DISK_TYPE == "IDE" ]]; then
	DISK_ARGS="-device piix3-ide,id=ide -device ide-hd,drive=disk,bus=ide.0"
elif [[ $BANAN_DISK_TYPE == "USB" ]]; then
	DISK_ARGS="-device usb-storage,drive=disk"
else
	DISK_ARGS="-device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0"
fi

QEMU_ARCH=$BANAN_ARCH
if [ $BANAN_ARCH = "i686" ]; then
	QEMU_ARCH=i386
fi

qemu-system-$QEMU_ARCH												\
	-m 1G															\
	-smp 4															\
	$BIOS_ARGS														\
	-drive format=raw,id=disk,file=${BANAN_DISK_IMAGE_PATH},if=none	\
	-device e1000e,netdev=net										\
	-netdev user,id=net												\
	-device qemu-xhci -device usb-kbd -device usb-mouse				\
	$DISK_ARGS														\
	$@																\
