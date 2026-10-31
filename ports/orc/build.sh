#!/bin/bash ../install.sh

NAME='orc'
VERSION='0.4.44'
DOWNLOAD_URL="https://gitlab.freedesktop.org/gstreamer/orc/-/archive/$VERSION/orc-$VERSION.tar.gz#f497abd377b0784913a1052b3d6ca324821564ae7b9aa5d55cede5705a177c15"
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dbuildtype=release'
	'-Dtests=disabled'
	'-Dexamples=disabled'
	'-Dbenchmarks=disabled'
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
