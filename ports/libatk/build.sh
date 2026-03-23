#!/bin/bash ../install.sh

NAME='libatk'
VERSION='2.56.6'
DOWNLOAD_URL="https://gitlab.gnome.org/GNOME/at-spi2-core/-/archive/$VERSION/at-spi2-core-$VERSION.tar.gz#49b1a640d50a95389a31672a0a077f0c20e8e222322cbd0228d3fa597686819d"
TAR_CONTENT="at-spi2-core-$VERSION"
DEPENDENCIES=('glib' 'dbus' 'libxml2' 'libX11' 'libXtst')
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
