#!/bin/bash ../install.sh

NAME='cairo'
VERSION='1.18.4'
DOWNLOAD_URL="https://cairographics.org/releases/cairo-$VERSION.tar.xz#445ed8208a6e4823de1226a74ca319d3600e83f6369f99b14265006599c32ccb"
DEPENDENCIES=('libpng' 'pixman' 'fontconfig' 'glib')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dtests=disabled'
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
