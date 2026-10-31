#!/bin/bash ../install.sh

NAME='libaom'
VERSION='3.15.3'
DOWNLOAD_URL="https://storage.googleapis.com/aom-releases/libaom-$VERSION.tar.gz#12c01dc731ed7b5a999e28120b45bf58a002ba3435a4ba2cfbc1e084a27c9c04"

configure() {
	cmake --fresh -B build -S . -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=ON \
		-DENABLE_TESTS=OFF \
		-DENABLE_EXAMPLES=OFF \
		|| exit 1
}

build() {
	cmake --build build || exit 1
}

install() {
	DESTDIR="$DESTDIR" cmake --install build || exit 1
	rm -f "$DESTDIR/usr/lib/libaom.a"
}
