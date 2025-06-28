#!/bin/bash ../install.sh

NAME='bash'
VERSION='5.2.37'
DOWNLOAD_URL="https://ftp.gnu.org/gnu/bash/bash-$VERSION.tar.gz#9599b22ecd1d5787ad7d3b7bf0c59f312b3396d1e281175dd1f8a4014da621ff"
DEPENDENCIES=('ncurses')
CONFIGURE_OPTIONS=(
	'--disable-nls'
	'--without-bash-malloc'
	'--with-curses'
	'bash_cv_unusable_rtsigs=no'
	'bash_cv_signal_vintage=posix'
	'CFLAGS=-std=c17'
	'CFLAGS_FOR_BUILD=-std=c17'
)

post_install() {
	if [ ! -L $BANAN_SYSROOT/usr/bin/sh ]; then
		ln -s bash $BANAN_SYSROOT/usr/bin/sh
	fi
}
