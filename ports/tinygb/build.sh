#!/bin/bash ../install.sh

NAME='tinygb'
VERSION='git'
DOWNLOAD_URL="https://github.com/jewelcodes/tinygb.git#57fdaff675a6b5b963b2b6624868d9698eabe375"

configure() {
	:
}

build() {
	make -f Makefile.banan_os -j$(nproc) || exit 1
}

install() {
	cp -v tinygb "$BANAN_SYSROOT"/usr/bin || exit 1
}
