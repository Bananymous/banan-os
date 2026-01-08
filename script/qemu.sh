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
DISK_ARGS="-drive format=raw,id=disk,file=${BANAN_DISK_IMAGE_PATH},if=none $DISK_ARGS"

QEMU_ARCH=$BANAN_ARCH
if [ $BANAN_ARCH = "i686" ]; then
	QEMU_ARCH=i386
fi

NET_ARGS='-netdev user,id=net'
NET_ARGS="-device e1000e,netdev=net $NET_ARGS"

USB_ARGS='-device qemu-xhci -device usb-kbd,port=1 -device usb-hub,port=2 -device usb-tablet,port=2.1'

#SOUND_ARGS='-device ac97'
SOUND_ARGS='-device intel-hda -device hda-output'

if [[ $@ == *"-accel kvm"* ]]; then
	CPU_ARGS='-cpu host,migratable=off'
fi

qemu-system-$QEMU_ARCH \
	-m 1G -smp 4       \
	$CPU_ARGS          \
	$BIOS_ARGS         \
	$USB_ARGS          \
	$DISK_ARGS         \
	$NET_ARGS          \
	$SOUND_ARGS        \
	$@                 \
