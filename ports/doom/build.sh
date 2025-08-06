#!/bin/bash ../install.sh

NAME='doom'
VERSION='git'
DOWNLOAD_URL="https://github.com/ozkl/doomgeneric.git#5041246e859052e2e258ca6edb4e1e9bbd98fcf5"
DEPENDENCIES=('SDL2' 'SDL2_mixer' 'timidity')

configure() {
	rm -rf doomgeneric/build
}

build() {
	if [ ! -f ../doom1.wad ]; then
		wget https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad -O ../doom1.wad || exit 1
	fi

	CFLAGS='-std=c11' make --directory doomgeneric --file Makefile.sdl CC="$CC" SDL_PATH="$BANAN_SYSROOT/usr/bin/" || exit 1
}

install() {
	cp doomgeneric/doomgeneric "${BANAN_SYSROOT}/bin/doom" || exit 1
	cp ../doom1.wad "$BANAN_SYSROOT/home/user/" || exit 1
}
