#!/bin/bash ../install.sh

NAME='xz'
VERSION='5.8.2'
DOWNLOAD_URL="https://github.com/tukaani-project/xz/releases/download/v5.8.2/xz-$VERSION.tar.xz#890966ec3f5d5cc151077879e157c0593500a522f413ac50ba26d22a9a145214"

configure() {
	cmake --fresh -B build -S . -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=ON \
		-DXZ_NLS=OFF \
		-DXZ_DOC=OFF \
		 || exit 1
}

build() {
	cmake --build build ||exit 1
}

install() {
	cmake --install build ||exit 1
}
