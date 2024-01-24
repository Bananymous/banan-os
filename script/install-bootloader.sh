#!/bin/bash

set -e

if [[ -z $BANAN_DISK_IMAGE_PATH ]]; then
	echo "You must set the BANAN_DISK_IMAGE_PATH environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_BUILD_DIR ]]; then
	echo "You must set the BANAN_BUILD_DIR environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_ROOT_DIR ]]; then
	echo "You must set the BANAN_ROOT_DIR environment variable" >&2
	exit 1
fi

if [[ -z $CMAKE_COMMAND ]]; then
	echo "You must set the CMAKE_COMMAND environment variable" >&2
	exit 1
fi

ROOT_PARTITION_INDEX=2
ROOT_PARTITION_INFO=$(fdisk -x $BANAN_DISK_IMAGE_PATH | grep "^$BANAN_DISK_IMAGE_PATH" | head -$ROOT_PARTITION_INDEX | tail -1)
ROOT_PARTITION_GUID=$(echo $ROOT_PARTITION_INFO | cut -d' ' -f6)

INSTALLER_BUILD_DIR=$BANAN_ROOT_DIR/bootloader/installer/build
BOOTLOADER_ELF=$BANAN_BUILD_DIR/bootloader/bios/bootloader

if ! [ -f $BOOTLOADER_ELF ]; then
	echo "You must build the bootloader first" >&2
	exit 1
fi

if ! [ -d $INSTALLER_BUILD_DIR ]; then
	mkdir -p $INSTALLER_BUILD_DIR
	cd $INSTALLER_BUILD_DIR
	$CMAKE_COMMAND ..
fi

cd $INSTALLER_BUILD_DIR
make

echo installing bootloader
$INSTALLER_BUILD_DIR/x86_64-banan_os-bootloader-installer $BOOTLOADER_ELF $BANAN_DISK_IMAGE_PATH $ROOT_PARTITION_GUID
