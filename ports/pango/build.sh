#!/bin/bash ../install.sh

NAME='pango'
VERSION='1.57.0'
DOWNLOAD_URL="https://gitlab.gnome.org/GNOME/pango/-/archive/$VERSION/pango-$VERSION.tar.gz#1b2e2f683dfb5adec3faf17087ade8c648f10a5d3d0e17e421e0ac1a39e6740e"
DEPENDENCIES=('glib' 'fontconfig' 'cairo' 'fribidi')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dbuild-testsuite=false'
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
