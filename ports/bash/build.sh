#!/bin/bash ../install.sh

NAME='bash'
VERSION='5.3'
DOWNLOAD_URL="https://ftpmirror.gnu.org/gnu/bash/bash-$VERSION.tar.gz#0d5cd86965f869a26cf64f4b71be7b96f90a3ba8b3d74e27e8e9d9d5550f31ba"
DEPENDENCIES=('ncurses')
CONFIG_SUB=('support/config.sub')
CONFIGURE_OPTIONS=(
	'--disable-nls'
	'--without-bash-malloc'
	'--with-curses'
	'bash_cv_unusable_rtsigs=no'
)

post_install() {
	ln -svf bash "$DESTDIR/usr/bin/sh"
}
