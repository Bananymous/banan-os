#!/bin/bash ../install.sh

NAME='glu'
VERSION='9.0.3'
DOWNLOAD_URL="https://archive.mesa3d.org/glu/glu-$VERSION.tar.xz#bd43fe12f374b1192eb15fe20e45ff456b9bc26ab57f0eee919f96ca0f8a330f"
DEPENDENCIES=('mesa')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
)

configure() {
	meson setup --reconfigure --cross-file "$MESON_CROSS_FILE" "${CONFIGURE_OPTIONS[@]}" build || exit 1
}

build() {
	meson compile -C build || exit 1
}

install() {
	meson install --destdir="$BANAN_SYSROOT" -C build || exit 1
}
