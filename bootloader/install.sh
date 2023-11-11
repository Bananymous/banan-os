#!/bin/sh

set -e

if [[ -z $BANAN_DISK_IMAGE_PATH ]]; then
	echo "You must set the BANAN_DISK_IMAGE_PATH environment variable" >&2
	exit 1
fi

CURRENT_DIR=$(dirname $(realpath $0))

INSTALLER_DIR=$CURRENT_DIR/installer
INSTALLER_BUILD_DIR=$INSTALLER_DIR/build

BUILD_DIR=$CURRENT_DIR/build

if ! [ -d $INSTALLER_BUILD_DIR ]; then
	mkdir -p $INSTALLER_BUILD_DIR
	cd $INSTALLER_BUILD_DIR
	cmake ..
fi

cd $INSTALLER_BUILD_DIR
make

mkdir -p $BUILD_DIR

echo compiling bootloader
x86_64-banan_os-as $CURRENT_DIR/arch/x86_64/boot.S -o $BUILD_DIR/bootloader.o

echo linking bootloader
x86_64-banan_os-ld -nostdlib -T $CURRENT_DIR/arch/x86_64/linker.ld $BUILD_DIR/bootloader.o -o $BUILD_DIR/bootloader

echo installing bootloader
$INSTALLER_BUILD_DIR/x86_64-banan_os-bootloader-installer $BUILD_DIR/bootloader $BANAN_DISK_IMAGE_PATH
