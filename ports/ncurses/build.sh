#!/bin/bash ../install.sh

NAME='ncurses'
VERSION='6.5'
DOWNLOAD_URL="https://ftp.gnu.org/gnu/ncurses/ncurses-$VERSION.tar.gz#136d91bc269a9a5785e5f9e980bc76ab57428f604ce3e5a5a90cebc767971cc6"
CONFIGURE_OPTIONS=(
	"--with-pkg-config='$PKG_CONFIG'"
	"--with-pkg-config-libdir=/usr/lib/pkgconfig"
	'--enable-pc-files'
	'--enable-sigwinch'
	'--disable-widec'
	'--with-shared'
	'--without-ada'
	'--without-manpages'
	'--without-dlsym'
	'--without-cxx-binding'
)

install() {
	make install "DESTDIR=$BANAN_SYSROOT" || exit 1

	shellrc="$BANAN_SYSROOT/home/user/.shellrc"
	grep -q 'export NCURSES_NO_UTF8_ACS=' "$shellrc" || echo 'export NCURSES_NO_UTF8_ACS=1' >> "$shellrc"
}
