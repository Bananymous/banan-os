#!/bin/bash ../install.sh

NAME='libyuv'
VERSION='git'
DOWNLOAD_URL="https://chromium.googlesource.com/libyuv/libyuv.git#eb6e7bb63738e29efd82ea3cf2a115238a89fa51"
DEPENDENCIES=('libjpeg-turbo')

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
	rm -f "$DESTDIR/usr/lib/libyuv.a"

	mkdir -p "$DESTDIR/usr/share/licenses" || exit 1
	cp -v LICENSE "$DESTDIR/usr/share/licenses/$NAME" || exit 1
}
