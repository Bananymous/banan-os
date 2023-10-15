#!/bin/bash
set -e

if [ -z ${OVMF_PATH+x} ]; then
	OVMF_PATH="/usr/share/ovmf/x64/OVMF.fd"
fi

if [ "$UEFI_BOOT" == "1" ]; then
	BIOS_ARGS="-bios $OVMF_PATH -net none"
fi

qemu-system-$BANAN_ARCH											\
	-m 128														\
	-smp 2														\
	$BIOS_ARGS													\
	-drive format=raw,id=disk,file=${DISK_IMAGE_PATH},if=none	\
	-device ahci,id=ahci										\
	-device ide-hd,drive=disk,bus=ahci.0						\
	$@															\
