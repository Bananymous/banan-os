#!/bin/bash ../install.sh

NAME='libxml2'
VERSION='2.15.1'
DOWNLOAD_URL="https://gitlab.gnome.org/GNOME/libxml2/-/archive/v$VERSION/libxml2-v$VERSION.tar.gz#0a5ebf8fa131585748d661ae692503483aff39d9b29b6c4b342cd80d422f246c"
TAR_CONTENT="libxml2-v$VERSION"
DEPENDENCIES=('zlib' 'libiconv')
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
