#!/bin/bash ../install.sh

NAME='vim'
VERSION='9.1.1485'
DOWNLOAD_URL="https://github.com/vim/vim/archive/refs/tags/v$VERSION.tar.gz#89b48e30c9e97bb819ffed752c8a1727b70bed79890bffe9da5f7c2170487dd2"
DEPENDENCIES=('ncurses')
CONFIGURE_OPTIONS=(
	'--with-tlib=ncurses'
	'--disable-nls'
	'--disable-sysmouse'
	'--disable-channel'
	'vim_cv_toupper_broken=no'
	'vim_cv_terminfo=yes'
	'vim_cv_tgetent=yes'
	'vim_cv_getcwd_broken=no'
	'vim_cv_stat_ignores_slash=yes'
	'vim_cv_memmove_handles_overlap=yes'
	'CFLAGS=-Wno-incompatible-pointer-types'
)

post_configure() {
	# vim doesn't do link tests, so it thinks these exists
	config_undefines=(
		'HAVE_SHM_OPEN'
		'HAVE_TIMER_CREATE'
	)

	for undefine in "${config_undefines[@]}"; do
		sed -i "s|^#define $undefine 1$|/\* #undef $undefine \*/|" src/auto/config.h
	done
}

post_install() {
	shellrc="$BANAN_SYSROOT/home/user/.shellrc"
	grep -q 'export EDITOR=' "$shellrc" || echo 'export EDITOR=vim' >> "$shellrc"
}
