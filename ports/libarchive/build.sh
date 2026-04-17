#!/bin/bash ../install.sh

NAME='libarchive'
VERSION='3.8.6'
DOWNLOAD_URL="https://github.com/libarchive/libarchive/releases/download/v$VERSION/libarchive-$VERSION.tar.xz#8ac57c1f5e99550948d1fe755c806d26026e71827da228f36bef24527e372e6f"
DEPENDENCIES=('zlib' 'zstd' 'bzip2' 'xz')

configure() {
	cmake --fresh -B build -S . -G Ninja  \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DENABLE_TEST=OFF \
		|| exit 1
}

build() {
	cmake --build build || exit 1
}

install() {
	cmake --install build || exit 1
}
