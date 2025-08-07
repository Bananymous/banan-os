#!/bin/bash ../install.sh

NAME='tinygb'
VERSION='git'
DOWNLOAD_URL="https://github.com/jewelcodes/tinygb.git#57fdaff675a6b5b963b2b6624868d9698eabe375"
DEPENDENCIES=('SDL2')

configure() {
	sed -i "s|shell sdl2-config|shell $BANAN_SYSROOT/usr/bin/sdl2-config|g" Makefile
	make clean
}

build() {
	make CC="$CC" LD="$CC" || exit 1
}

install() {
	cp -v tinygb "$BANAN_SYSROOT"/usr/bin || exit 1
}
