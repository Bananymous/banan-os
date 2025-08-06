#!/bin/bash ../install.sh

NAME='SDL2_mixer'
VERSION='2.8.1'
DOWNLOAD_URL="https://github.com/libsdl-org/SDL_mixer/releases/download/release-$VERSION/SDL2_mixer-$VERSION.tar.gz#cb760211b056bfe44f4a1e180cc7cb201137e4d1572f2002cc1be728efd22660"
DEPENDENCIES=('SDL2')

configure() {
	$BANAN_CMAKE --fresh -S . -B build -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX='/usr' \
		-DCMAKE_BUILD_TYPE=Release \
		-DSDL2MIXER_WAVPACK=OFF \
		-DSDL2MIXER_MIDI=OFF \
		-DSDL2MIXER_OPUS=OFF \
		-DSDL2MIXER_MOD=OFF \
		 || exit 1
}

build() {
	$BANAN_CMAKE --build build || exit 1
}

install() {
	$BANAN_CMAKE --install build || exit 1
}
