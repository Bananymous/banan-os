#!/bin/bash ../install.sh

NAME='zstd'
VERSION='1.5.7'
DOWNLOAD_URL="https://github.com/facebook/zstd/releases/download/v$VERSION/zstd-$VERSION.tar.gz#eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3"

configure() {
	$BANAN_CMAKE -B _build -S build/cmake -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		 || exit 1
}

build() {
	$BANAN_CMAKE --build _build ||exit 1
}

install() {
	$BANAN_CMAKE --install _build ||exit 1
}
