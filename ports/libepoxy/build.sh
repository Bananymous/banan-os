#!/bin/bash ../install.sh

NAME='libepoxy'
VERSION='1.5.10'
DOWNLOAD_URL="https://download.gnome.org/sources/libepoxy/1.5/libepoxy-$VERSION.tar.xz#072cda4b59dd098bba8c2363a6247299db1fa89411dc221c8b81b8ee8192e623"
DEPENDENCIES=('mesa' 'libX11')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dtests=false'
	'-Degl=no'
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
