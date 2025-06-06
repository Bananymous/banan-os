#!/bin/bash ../install.sh

NAME='SpecSeek'
VERSION='git'
DOWNLOAD_URL="https://github.com/Mellurboo/SpecSeek.git#f9dc4d1d192e2a7ac0dfecd2e701a5ef57e6b8b3"

if [ $BANAN_ARCH = "x86_64" ]; then
	specseek_arch=64
elif [ $BANAN_ARCH = "i686" ]; then
	specseek_arch=32
fi

configure() {
	make clean || exit 1
}

build() {
	make specseek_$specseek_arch || exit 1
}

install() {
	cp -v bin/gcc/$specseek_arch/specseek_$specseek_arch "$BANAN_SYSROOT/usr/bin/specseek" || exit 1
}
