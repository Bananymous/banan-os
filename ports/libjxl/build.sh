#!/bin/bash ../install.sh

NAME='libjxl'
VERSION='0.12.0'
DOWNLOAD_URL="https://github.com/libjxl/libjxl/archive/refs/tags/v$VERSION.tar.gz#03e9be69a30be4011f559da75328b6d7cea8ad921fabfbd551ce10bf45cdc992"
DEPENDENCIES=('brotli' 'giflib' 'highway' 'lcms2' 'libjpeg-turbo' 'libpng' 'libwebp' 'zlib')

configure() {
	cmake --fresh -B build -S . -G Ninja  \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_TESTING=OFF \
		-DJPEGXL_ENABLE_BENCHMARK=OFF \
		-DJPEGXL_ENABLE_EXAMPLES=OFF \
		-DJPEGXL_ENABLE_SKCMS=OFF \
		-DJPEGXL_ENABLE_SJPEG=OFF \
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
