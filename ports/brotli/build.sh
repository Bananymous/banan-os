#!/bin/bash ../install.sh

NAME='brotli'
VERSION='1.2.1'
DOWNLOAD_URL="https://github.com/google/brotli/archive/refs/tags/v$VERSION.tar.gz#98d35d11a78dc453aebb7d10c3748f3f73ff78c8267c0dc697b8631f67bf7e8f"

configure() {
	cmake --fresh -B build -S . -G Ninja  \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DBROTLI_DISABLE_TESTS=ON \
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
