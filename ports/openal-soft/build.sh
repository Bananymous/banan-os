#!/bin/bash ../install.sh

NAME='openal-soft'
VERSION='1.24.3'
DOWNLOAD_URL="https://github.com/kcat/openal-soft/archive/refs/tags/$VERSION.tar.gz#7e1fecdeb45e7f78722b776c5cf30bd33934b961d7fd2a11e0494e064cc631ce"
DEPENDENCIES=('SDL2' 'zlib' 'libsndfile')

configure() {
	cmake --fresh -B build -S . -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DALSOFT_EXAMPLES=OFF \
		-DALSOFT_NO_CONFIG_UTIL=ON \
		-DALSOFT_BACKEND_SDL2=ON \
		|| exit 1
}

build() {
	cmake --build build || exit 1
}

install() {
	cmake --install build || exit 1
}
