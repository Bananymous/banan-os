#!/bin/bash ../install.sh

NAME='libXft'
VERSION='2.3.9'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXft-$VERSION.tar.xz#60a25b78945ed6932635b3bb1899a517d31df7456e69867ffba27f89ff3976f5"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libXrender' 'libX11' 'freetype' 'fontconfig')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
