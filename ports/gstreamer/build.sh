#!/bin/bash ../install.sh

NAME='gstreamer'
VERSION='1.28.8'
DOWNLOAD_URL="https://gitlab.freedesktop.org/gstreamer/gstreamer/-/archive/$VERSION/gstreamer-$VERSION.tar.gz#fd146dc03ea55ccfe78c1a91d35b71f20089209e173163f98237aad9f5ec7540"
DEPENDENCIES=(
	'cairo'
	'ffmpeg'
	'flac'
	'glib'
	'gmp'
	'json-glib'
	'libaom'
	'libjpeg-turbo'
	'libogg'
	'libsoup'
	'libvorbis'
	'libvpx'
	'libxml2'
	'mesa-glx'
	'openal-soft'
	'opus'
	'orc'
	'pango'
)
CONFIGURE_OPTIONS=(
	'--wrap-mode=nodownload'
	'-Dprefix=/usr'
	'-Dbuildtype=release'
	'-Dtests=disabled'
	'-Dexamples=disabled'
	'-Dbenchmarks=disabled'
	'-Dgpl=enabled'
	'-Dorc-source=system'
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
