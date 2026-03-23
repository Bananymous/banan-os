#!/bin/bash ../install.sh

NAME='xbanan'
VERSION='git'
DOWNLOAD_URL="https://git.bananymous.com/Bananymous/xbanan.git#b228ef13c41adff2738acaeda5db804ebf493bfd"
DEPENDENCIES=('mesa' 'libX11' 'xorgproto')

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
