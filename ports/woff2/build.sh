#!/bin/bash ../install.sh

NAME='woff2'
VERSION='1.0.2'
DOWNLOAD_URL="https://github.com/google/woff2/archive/refs/tags/v$VERSION.tar.gz#add272bb09e6384a4833ffca4896350fdb16e0ca22df68c0384773c67a175594"
DEPENDENCIES=('brotli')

configure() {
	cmake --fresh -B build -S . -G Ninja  \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		|| exit 1
}

build() {
	cmake --build build || exit 1
}

install() {
	DESTDIR="$DESTDIR" cmake --install build || exit 1

	mkdir -p "$DESTDIR/usr/share/licenses" || exit 1
	cp -v LICENSE "$DESTDIR/usr/share/licenses/$NAME" || exit 1
}
