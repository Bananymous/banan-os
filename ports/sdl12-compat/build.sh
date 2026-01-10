#!/bin/bash ../install.sh

NAME='sdl12-compat'
VERSION='1.2.68'
DOWNLOAD_URL="https://github.com/libsdl-org/sdl12-compat/archive/refs/tags/release-$VERSION.tar.gz#63c6e4dcc1154299e6f363c872900be7f3dcb3e42b9f8f57e05442ec3d89d02d"
TAR_CONTENT="sdl12-compat-release-$VERSION"
DEPENDENCIES=('SDL2' 'glu')

configure() {
	sed -i 's/CMAKE_INSTALL_FULL_DATAROOTDIR/CMAKE_INSTALL_FULL_DATADIR/' CMakeLists.txt

	cmake --fresh -B build -S . -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX="$BANAN_SYSROOT/usr" \
		-DSDL2_INCLUDE_DIR="$BANAN_SYSROOT/usr/include/SDL2"
}

build() {
	cmake --build build || exit 1
}

install() {
	cmake --install build || exit 1
}
