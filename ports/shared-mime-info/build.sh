#!/bin/bash ../install.sh

NAME='shared-mime-info'
VERSION='2.4'
DOWNLOAD_URL="https://gitlab.freedesktop.org/xdg/shared-mime-info/-/archive/$VERSION/shared-mime-info-$VERSION.tar.gz#531291d0387eb94e16e775d7e73788d06d2b2fdd8cd2ac6b6b15287593b6a2de"
DEPENDENCIES=('glib' 'libxml2')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dbuild-tests=false'
	'-Dbuild-translations=false'
	'-Dupdate-mimedb=true'
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
