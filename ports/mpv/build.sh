#!/bin/bash ../install.sh

NAME='mpv'
VERSION='0.41.0'
DOWNLOAD_URL="https://github.com/mpv-player/mpv/archive/refs/tags/v$VERSION.tar.gz#ee21092a5ee427353392360929dc64645c54479aefdb5babc5cfbb5fad626209"
DEPENDENCIES=(
	'curl'
	'ffmpeg'
	'lcms2'
	'libarchive'
	'libass'
	'libiconv'
	'libjpeg-turbo'
	'libplacebo'
	'sdl2-compat'
	'luajit'
)
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dbuildtype=release'
	'-Db_lundef=true'
	'-Dsdl2-audio=enabled'
	'-Dsdl2-video=enabled'
	'-Dlua=luajit'
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
