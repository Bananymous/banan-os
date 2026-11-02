#!/bin/bash ../install.sh

NAME='glu'
VERSION='9.0.3'
DOWNLOAD_URL="https://archive.mesa3d.org/glu/glu-$VERSION.tar.xz#bd43fe12f374b1192eb15fe20e45ff456b9bc26ab57f0eee919f96ca0f8a330f"
DEPENDENCIES=('mesa-osmesa')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dbuildtype=release'
	'-Ddefault_library=shared'
	'-Dgl_provider=osmesa'
)

configure() {
	meson setup --reconfigure --cross-file "$MESON_CROSS_FILE" "${CONFIGURE_OPTIONS[@]}" build || exit 1
}

build() {
	meson compile -C build || exit 1
}

install() {
	meson install --destdir="$DESTDIR" -C build || exit 1
}
