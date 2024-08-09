#!/bin/bash ../install.sh

NAME='vim'
VERSION='9.0'
DOWNLOAD_URL="ftp://ftp.vim.org/pub/vim/unix/vim-$VERSION.tar.bz2#a6456bc154999d83d0c20d968ac7ba6e7df0d02f3cb6427fb248660bacfb336e"
TAR_CONTENT='vim90'
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
)
