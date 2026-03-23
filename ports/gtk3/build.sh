#!/bin/bash ../install.sh

NAME='gtk'
VERSION='3.24.49'
DOWNLOAD_URL="https://gitlab.gnome.org/GNOME/gtk/-/archive/$VERSION/gtk-$VERSION.tar.gz#a2958d82986c81794e953a3762335fa7c78948706d23cced421f7245ca544cbc"
DEPENDENCIES=('glib' 'gdk-pixbuf' 'pango' 'libatk' 'libepoxy' 'libXrandr')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dtests=false'
	'-Dexamples=false'
	'-Dintrospection=false'
	'-Dx11_backend=true'
	'-Dwayland_backend=false'
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
