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

qemu-system-$BANAN_ARCH												\
	-m 128															\
	-smp 2															\
	$BIOS_ARGS														\
	-drive format=raw,id=disk,file=${BANAN_DISK_IMAGE_PATH},if=none	\
	-device ahci,id=ahci											\
	-device ide-hd,drive=disk,bus=ahci.0							\
	$@																\
