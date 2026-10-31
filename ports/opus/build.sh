#!/bin/bash ../install.sh

NAME='opus'
VERSION='1.6'
DOWNLOAD_URL="https://downloads.xiph.org/releases/opus/opus-$VERSION.tar.gz#b7637334527201fdfd6dd6a02e67aceffb0e5e60155bbd89175647a80301c92c"
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dbuildtype=release'
	'-Dtests=disabled'
	'-Ddocs=disabled'
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
