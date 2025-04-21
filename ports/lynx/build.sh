#!/bin/bash ../install.sh

NAME='lynx'
VERSION='2.9.2'
DOWNLOAD_URL="https://invisible-island.net/archives/lynx/tarballs/lynx$VERSION.tar.gz#99f8f28f860094c533100d1cedf058c27fb242ce25e991e2d5f30ece4457a3bf"
TAR_CONTENT="lynx$VERSION"
DEPENDENCIES=('ncurses' 'ca-certificates' 'openssl' 'zlib')
CONFIGURE_OPTIONS=(
	'--without-system-type'
	'--with-sceen=ncurses'
	'--with-ssl'
	'--with-zlib'
)
