#!/bin/sh
set -e
 
qemu-system-$BANAN_ARCH										\
	-m 128													\
	-smp 2													\
	-drive format=raw,media=disk,file=${DISK_IMAGE_PATH}	\
	-serial stdio											\
	-accel kvm												\
