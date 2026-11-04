#!/bin/bash ../install.sh

NAME='fakeroot'
VERSION='2.1.4'
DOWNLOAD_URL="https://salsa.debian.org/clint/fakeroot/-/archive/upstream/$VERSION/fakeroot-upstream-$VERSION.tar.gz#b71f645037e5a44a474efd641b353b506db3803eb60ed90a4ac6322a8d96d340"
TAR_CONTENT="fakeroot-upstream-$VERSION"
DEPENDENCIES=('bash')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dbuildtype=release'
	'-Dsh=/usr/bin/bash'
	'-Dipc=tcp'
)

pre_configure() {
	echo '' > test/meson.build
}

configure() {
	meson setup \
		--reconfigure \
		--cross-file "$MESON_CROSS_FILE" \
		"${CONFIGURE_OPTIONS[@]}" \
		build || exit 1
}

post_configure() {
	sed -E -i 's|^/\* #undef (NO_WRAP_.*64_SYMBOL) \*/$|#define \1 1|' build/config.h
}

build() {
	meson compile -C build || exit 1
}

install() {
	meson install --destdir="$DESTDIR" -C build || exit 1
}
