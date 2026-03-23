#!/bin/bash ../install.sh

NAME='fontconfig'
VERSION='2.17.1'
DOWNLOAD_URL="https://gitlab.freedesktop.org/fontconfig/fontconfig/-/archive/$VERSION/fontconfig-$VERSION.tar.gz#82e73b26adad651b236e5f5d4b3074daf8ff0910188808496326bd3449e5261d"
DEPENDENCIES=('harfbuzz' 'freetype' 'expat' 'libiconv')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dtests=disabled'
	'-Dnls=disabled'
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
	pushd .. &>/dev/null

	local font_name='dejavu-fonts-ttf-2.37'

	if [ ! -f "$font_name.tar.bz2" ]; then
		wget "http://sourceforge.net/projects/dejavu/files/dejavu/2.37/$font_name.tar.bz2" || exit 1
	fi

	if [ ! -d "$font_name" ]; then
		tar xf "$font_name.tar.bz2" || exit 1
	fi

	mkdir -p "$BANAN_SYSROOT/usr/share/fonts/TTF" || exit 1
	cp "$font_name/ttf/"* "$BANAN_SYSROOT/usr/share/fonts/TTF/" || exit 1
	cp "$font_name/fontconfig/"* "$BANAN_SYSROOT/usr/share/fontconfig/conf.avail/" || exit 1

	popd &>/dev/null
}
