#!/bin/bash ../install.sh

NAME='harfbuzz'
VERSION='12.2.0'
DOWNLOAD_URL="https://github.com/harfbuzz/harfbuzz/releases/download/$VERSION/harfbuzz-$VERSION.tar.xz#ecb603aa426a8b24665718667bda64a84c1504db7454ee4cadbd362eea64e545"
DEPENDENCIES=('glib' 'freetype')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dtests=disabled'
	'-Dcairo=disabled'
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

post_install() {
	for circular in freetype cairo; do
		pushd "$BANAN_PORT_DIR/$circular" >/dev/null || exit 1
		rm -f .compile_hash
		./build.sh || exit 1
		popd >/dev/null
	done
}
