#!/bin/sh

set -e

CURRENT_DIR=$(dirname $(realpath $0))

INSTALLER_DIR=$CURRENT_DIR/installer
INSTALLER_BUILD_DIR=$INSTALLER_DIR/build

BUILD_DIR=$CURRENT_DIR/build
DISK_IMAGE_PATH=$CURRENT_DIR/test.img

if ! [ -d $INSTALLER_BUILD_DIR ]; then
	mkdir -p $INSTALLER_BUILD_DIR
	cd $INSTALLER_BUILD_DIR
	cmake ..
fi

cd $INSTALLER_BUILD_DIR
make

cd $CURRENT_DIR

echo creating clean disk image
truncate --size 0 $DISK_IMAGE_PATH
truncate --size 50M $DISK_IMAGE_PATH
echo -ne 'g\nn\n\n\n+1M\nt 1\n4\nw\n' | fdisk $DISK_IMAGE_PATH > /dev/null

mkdir -p $BUILD_DIR

echo compiling bootloader
x86_64-banan_os-as arch/x86_64/boot.S -o $BUILD_DIR/bootloader.o

echo linking bootloader
x86_64-banan_os-ld -nostdlib -T arch/x86_64/linker.ld $BUILD_DIR/bootloader.o -o $BUILD_DIR/bootloader

echo installing bootloader
$INSTALLER_BUILD_DIR/x86_64-banan_os-bootloader-installer $BUILD_DIR/bootloader $DISK_IMAGE_PATH

if [ "$1" == "debug" ] ; then
	QEMU_FLAGS="-s -S"
fi

echo running qemu
qemu-system-x86_64 $QEMU_FLAGS --drive format=raw,file=test.img
