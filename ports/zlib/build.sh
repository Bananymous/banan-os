#!/bin/bash ../install.sh

NAME='zlib'
VERSION='1.3.2'
DOWNLOAD_URL="https://www.zlib.net/zlib-$VERSION.tar.gz#bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16"

configure() {
	cmake --fresh -B build -S . -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		|| exit 1
}

build() {
	cmake --build build || exit 1
}

install() {
	cmake --install build || exit 1
}
