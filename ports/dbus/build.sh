#!/bin/bash ../install.sh

NAME='dbus'
VERSION='1.16.2'
DOWNLOAD_URL="https://dbus.freedesktop.org/releases/dbus/dbus-$VERSION.tar.xz#0ba2a1a4b16afe7bceb2c07e9ce99a8c2c3508e5dec290dbb643384bd6beb7e2"
DEPENDENCIES=('glib' 'expat')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Depoll=disabled' # linux only
	'-Dintrusive_tests=false'
	'-Dinstalled_tests=false'
	'-Dmodular_tests=disabled'
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
