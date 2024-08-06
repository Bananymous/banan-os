#!/bin/bash ../install.sh

NAME='ncurses'
VERSION='6.5'
DOWNLOAD_URL="https://ftp.gnu.org/gnu/ncurses/ncurses-$VERSION.tar.gz#136d91bc269a9a5785e5f9e980bc76ab57428f604ce3e5a5a90cebc767971cc6"
CONFIGURE_OPTIONS=(
	'--disable-db-intall'
	'--disable-widec'
	'--without-ada'
	'--without-manpages'
	'--without-dlsym'
	'--without-cxx-binding'
)
