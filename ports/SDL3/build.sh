#!/bin/bash ../install.sh

NAME='SDL3'
VERSION='3.4.10'
DOWNLOAD_URL="https://github.com/libsdl-org/SDL/releases/download/release-$VERSION/SDL3-$VERSION.tar.gz#12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785"
DEPENDENCIES=('mesa-osmesa' 'libiconv')

configure() {
	cmake --fresh -S . -B build -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX='/usr' \
		-DCMAKE_BUILD_TYPE=Release \
		 || exit 1
}

build() {
	cmake --build build || exit 1
}

install() {
	DESTDIR="$DESTDIR" cmake --install build || exit 1
}
