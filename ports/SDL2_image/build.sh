#!/bin/bash ../install.sh

NAME='SDL2_image'
VERSION='2.8.8'
DOWNLOAD_URL="https://github.com/libsdl-org/SDL_image/releases/download/release-$VERSION/SDL2_image-$VERSION.tar.gz#2213b56fdaff2220d0e38c8e420cbe1a83c87374190cba8c70af2156097ce30a"
DEPENDENCIES=('SDL2' 'libpng' 'libjpeg' 'libtiff' 'libwebp')

configure() {
	$BANAN_CMAKE --fresh -S . -B build -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX='/usr' \
		-DCMAKE_BUILD_TYPE=Release \
		-DSDL2IMAGE_AVIF=OFF \
		 || exit 1
}

build() {
	$BANAN_CMAKE --build build || exit 1
}

install() {
	$BANAN_CMAKE --install build || exit 1
}
