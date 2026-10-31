#!/bin/bash ../install.sh

NAME='libsoup'
VERSION='3.6.6'
DOWNLOAD_URL="https://github.com/GNOME/libsoup/archive/refs/tags/$VERSION.tar.gz#828c357f6deca0646744ca2045870a8a215cd97a87357069b8fe94a786d8c316"
DEPENDENCIES=('glib' 'nghttp2' 'sqlite3' 'libpsl')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dbuildtype=release'
	'-Dtests=false'
	'-Ddocs=disabled'
	'-Dintrospection=disabled'
	'-Dsysprof=disabled'
	'-Dtls_check=false'
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
	meson install --destdir="$DESTDIR" -C build || exit 1
}
