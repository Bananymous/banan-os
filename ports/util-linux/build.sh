#!/bin/bash ../install.sh

NAME='util-linux'
VERSION='2.42.4'
DOWNLOAD_URL="https://github.com/util-linux/util-linux/archive/refs/tags/v$VERSION.tar.gz#e1d38037dab761a2d39114d88a6744ffd5a4576efa29fe41e1dfbd8453891fe7"
CONFIGURE_OPTIONS=(
	'-Dc_args=-Wno-format-security -Wno-unused-parameter -Wno-sign-compare -Wno-format'
	'-Dprefix=/usr'
	'-Dsbindir=bin'
	'-Dbuildtype=release'
	'-Dauto_features=disabled'
	'-Dprogram-tests=false'
	'-Dbuild-libuuid=enabled'
	'-Dbuild-mkfs=enabled'
)

configure() {
	meson setup \
		--reconfigure \
		--cross-file "$MESON_CROSS_FILE" \
		"${CONFIGURE_OPTIONS[@]}" \
		build || exit 1
}

post_configure() {
	sed -i 's/#define HAVE_NET_IF_H 1/#undef HAVE_NET_IF_H/' build/config.h
	sed -i 's/#define HAVE___PROGNAME 1/#undef HAVE___PROGNAME/' build/config.h
	sed -i 's|<sys/syslog.h>|<syslog.h>|g' lib/logindefs.c lib/configs.c lib/shells.c
}

build() {
	meson compile -C build || exit 1
}

install() {
	meson install --destdir="$DESTDIR" -C build || exit 1
}
