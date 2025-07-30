#!/bin/bash ../install.sh

NAME='libuv'
VERSION='1.51.0'
DOWNLOAD_URL="https://dist.libuv.org/dist/v$VERSION/libuv-v$VERSION.tar.gz#5f0557b90b1106de71951a3c3931de5e0430d78da1d9a10287ebc7a3f78ef8eb"
TAR_CONTENT="libuv-v$VERSION"

configure() {
	$BANAN_CMAKE \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-B build -GNinja --fresh \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DBUILD_TESTING=OFF \
		. || exit 1
}

build() {
	$BANAN_CMAKE --build build || exit 1
}

install() {
	$BANAN_CMAKE --install build || exit 1
}
