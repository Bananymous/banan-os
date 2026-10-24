#!/bin/bash ../install.sh

NAME='ncurses'
VERSION='6.6'
DOWNLOAD_URL="https://ftpmirror.gnu.org/gnu/ncurses/ncurses-$VERSION.tar.gz#355b4cbbed880b0381a04c46617b7656e362585d52e9cf84a67e2009b749ff11"
CONFIG_SUB=('config.sub')
CONFIGURE_OPTIONS=(
	"--with-pkg-config='$PKG_CONFIG'"
	'--with-pkg-config-libdir=/usr/lib/pkgconfig'
	'--enable-pc-files'
	'--enable-sigwinch'
	'--with-shared'
	'--with-cxx-shared'
	'--without-normal'
	'--without-debug'
	'--without-tests'
	'--without-ada'
	'--without-manpages'
	CFLAGS='-std=c17'
)

pre_configure() {
	unset TERMINFO TERMINFO_DIRS
}

post_install() {
	for lib in ncurses ncurses++ form panel menu; do
		ln -svf ${lib}w.pc "$DESTDIR/usr/lib/pkgconfig/$lib.pc"
	done

	ln -svf libncursesw.so "$DESTDIR/usr/lib/libncurses.so"

	mkdir -p "$DESTDIR/etc/profile.d"
	echo 'export NCURSES_NO_UTF8_ACS=1' > "$DESTDIR/etc/profile.d/ncurses.sh"

	cat > ansi-color.ti << EOF
ansi-color|ANSI terminal with 24-bit true color,
        use=ansi,
        colors#256, pairs#65536, Tc,
        setaf=\E[%?%p1%{8}%<%t3%p1%d%e%p1%{16}%<%t9%p1%{8}%-%d%e38;5;%p1%d%;m,
        setab=\E[%?%p1%{8}%<%t4%p1%d%e%p1%{16}%<%t10%p1%{8}%-%d%e48;5;%p1%d%;m,
        setrgbf=\E[38\:2\:\:%p1%d\:%p2%d\:%p3%dm,
        setrgbb=\E[48\:2\:\:%p1%d\:%p2%d\:%p3%dm,
EOF
	tic -x -o "$DESTDIR/usr/share/terminfo" ansi-color.ti || exit 1
}
