#!/bin/bash ../install.sh

NAME='SDL2'
VERSION='2.32.8'
DOWNLOAD_URL="https://github.com/libsdl-org/SDL/releases/download/release-$VERSION/SDL2-$VERSION.tar.gz#0ca83e9c9b31e18288c7ec811108e58bac1f1bb5ec6577ad386830eac51c787e"
DEPENDENCIES=('mesa')

configure() {
	$BANAN_CMAKE --fresh -S . -B build -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX='/usr' \
		-DCMAKE_BUILD_TYPE=Release \
		-DSDL_LIBSAMPLERATE=OFF \
		-DSDL_PTHREADS_SEM=OFF \
		 || exit 1
}

build() {
	$BANAN_CMAKE --build build || exit 1
}

install() {
	$BANAN_CMAKE --install build || exit 1
}
