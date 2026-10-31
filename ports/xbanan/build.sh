#!/bin/bash ../install.sh

NAME='xbanan'
VERSION='git'
DOWNLOAD_URL="https://git.bananymous.com/Bananymous/xbanan.git#dde9b4095d74f2c1785035d735b9f3ebb1290cf2"
DEPENDENCIES=('xorgproto')

configure() {
	cmake --fresh -B build -S . -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DPLATFORM=banan-os \
		-DFONT_PATH=/usr/share/fonts/X11 \
		|| exit 1
}

build() {
	cmake --build build --target xbanan || exit 1
}

install() {
	mkdir -p "$DESTDIR/usr/bin"
	cp -v build/xbanan/xbanan "$DESTDIR/usr/bin/" || exit 1

	mkdir -p "$DESTDIR/usr/share/fonts/X11"
	cp -r fonts/misc "$DESTDIR/usr/share/fonts/X11/" || exit 1
}

post_install() {
	mkdir -p "$DESTDIR/etc/profile.d"
	echo 'export DISPLAY=:69' > "$DESTDIR/etc/profile.d/xbanan.sh"
}
