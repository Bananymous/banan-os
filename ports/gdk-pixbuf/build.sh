#!/bin/bash ../install.sh

NAME='gdk-pixbuf'
VERSION='2.44.4'
DOWNLOAD_URL="https://gitlab.gnome.org/GNOME/gdk-pixbuf/-/archive/$VERSION/gdk-pixbuf-$VERSION.tar.gz#6de2f77d992155b4121d20036e7e986dfe595a0e654381cdd0d7257f493c208a"
DEPENDENCIES=('glib' 'libpng' 'libjpeg' 'libtiff' 'shared-mime-info')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dtests=false'
	'-Dman=false'
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
