#!/bin/bash ../install.sh

NAME='ninja'
VERSION='1.13.2'
DOWNLOAD_URL="https://github.com/ninja-build/ninja/archive/refs/tags/v$VERSION.tar.gz#974d6b2f4eeefa25625d34da3cb36bdcebe7fbce40f4c16ac0835fd1c0cbae17"

configure() {
	cmake --fresh -B build -S . -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_TESTING=OFF \
		. || exit 1
}

build() {
	cmake --build build || exit 1
}

install() {
	DESTDIR="$DESTDIR" cmake --install build || exit 1
}
