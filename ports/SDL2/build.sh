#!/bin/bash ../install.sh

NAME='SDL2'
VERSION='2.30.11'
DOWNLOAD_URL="https://github.com/libsdl-org/SDL/archive/refs/tags/release-$VERSION.tar.gz#cc6136dd964854e8846c679703322f3e2a341d27a06a53f8b3f642c26f1b0cfd"
TAR_CONTENT="SDL-release-$VERSION"
DEPENDENCIES=('mesa')

configure() {
	$BANAN_CMAKE \
		--toolchain="$CMAKE_TOOLCHAIN_FILE" \
		--fresh -GNinja -S . -B build \
		-DCMAKE_INSTALL_PREFIX='/usr' \
		-DCMAKE_BUILD_TYPE=Release \
		-DBANAN_OS=true \
		-DUNIX=true \
		-DSDL_LIBSAMPLERATE=OFF \
		-DSDL_PTHREADS_SEM=OFF
}

build() {
	$BANAN_CMAKE --build build --config Release || exit 1
}

install() {
	$BANAN_CMAKE --install build || exit 1
}
