#!/bin/bash ../install.sh

NAME='xbanan'
VERSION='git'
DOWNLOAD_URL="https://git.bananymous.com/Bananymous/xbanan.git#b2c642f03d2e498e9d6acd55cc89a5e76c220811"
DEPENDENCIES=('xorgproto')

configure() {
	cmake --fresh -B build -S . -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		|| exit 1
}

build() {
	cmake --build build --target xbanan || exit 1
}

install() {
	cp -v build/xbanan/xbanan "$BANAN_SYSROOT/usr/bin" || exit 1

	mkdir -p "$BANAN_SYSROOT/usr/share/fonts/X11"
	cp -r fonts/misc "$BANAN_SYSROOT/usr/share/fonts/X11/" || exit 1
}

post_install() {
	shellrc="$BANAN_SYSROOT/home/user/.shellrc"
	grep -q 'export DISPLAY=' "$shellrc" || echo 'export DISPLAY=:69' >> "$shellrc"
}
