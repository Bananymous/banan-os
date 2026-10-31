#!/bin/bash ../install.sh

NAME='libavif'
VERSION='1.4.2'
DOWNLOAD_URL="https://github.com/AOMediaCodec/libavif/archive/refs/tags/v$VERSION.tar.gz#2b645287340ba5a631d268b551dc2d72bd73ac33335962dd36dcdb6d8366921d"
DEPENDENCIES=('dav1d' 'libyuv')

configure() {
	cmake --fresh -B build -S . -G Ninja  \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DAVIF_CODEC_DAV1D=SYSTEM \
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
