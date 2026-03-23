#!/bin/bash ../install.sh

NAME='pixman'
VERSION='0.46.4'
DOWNLOAD_URL="https://cairographics.org/releases/pixman-$VERSION.tar.gz#d09c44ebc3bd5bee7021c79f922fe8fb2fb57f7320f55e97ff9914d2346a591c"
DEPENDENCIES=('glib' 'libpng')
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
