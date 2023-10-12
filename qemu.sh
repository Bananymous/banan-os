#!/bin/bash
set -e

qemu-system-$BANAN_ARCH											\
	-m 128														\
	-smp 2														\
	-drive format=raw,id=disk,file=${DISK_IMAGE_PATH},if=none	\
	-device ahci,id=ahci										\
	-device ide-hd,drive=disk,bus=ahci.0						\
	$@															\
