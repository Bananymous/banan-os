#!/bin/bash ../install.sh

NAME='xorgproto'
VERSION='2024.1'
DOWNLOAD_URL="https://www.x.org/releases/individual/proto/xorgproto-$VERSION.tar.xz#372225fd40815b8423547f5d890c5debc72e88b91088fbfb13158c20495ccb59"
DEPENDENCIES=('util-macros')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
)

configure() {
	meson setup \
		--reconfigure \
		--cross-file "$MESON_CROSS_FILE" \
		"${CONFIGURE_OPTIONS[@]}" \
		build || exit 1
}

build() {
	meson compile -C build || exit 1
}

install() {
	meson install --destdir="$BANAN_SYSROOT" -C build || exit 1
}
