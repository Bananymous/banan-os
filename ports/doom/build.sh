#!/bin/bash ../install.sh

NAME='doom'
VERSION='git'
DOWNLOAD_URL="https://github.com/ozkl/doomgeneric.git#613f870b6fa83ede448a247de5a2571092fa729d"

configure() {
	:
}

build() {
	if [ ! -f ../doom1.wad ]; then
		wget https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad -O ../doom1.wad || exit 1
	fi
	make --directory doomgeneric --file Makefile.banan_os -j$(nproc) || exit 1
}

install() {
	cp doomgeneric/build/doom "${BANAN_SYSROOT}/bin/" || exit 1
	cp ../doom1.wad "$BANAN_SYSROOT/home/user/" || exit 1
}
