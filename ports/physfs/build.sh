#!/bin/bash ../install.sh

NAME='physfs'
VERSION='3.2.0'
DOWNLOAD_URL="https://github.com/icculus/physfs/archive/refs/tags/release-$VERSION.tar.gz#1991500eaeb8d5325e3a8361847ff3bf8e03ec89252b7915e1f25b3f8ab5d560"
TAR_CONTENT="physfs-release-$VERSION"

configure() {
	$BANAN_CMAKE \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-B build -G Ninja --fresh . \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DPHYSFS_BUILD_TEST=ON \
		-DPHYSFS_BUILD_DOCS=OFF \
		|| exit 1
}

build() {
	$BANAN_CMAKE --build build || exit 1
}

install() {
	$BANAN_CMAKE --install build || exit 1
}
