#!/bin/bash ../install.sh

NAME='libXi'
VERSION='1.8.2'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXi-$VERSION.tar.xz#d0e0555e53d6e2114eabfa44226ba162d2708501a25e18d99cfb35c094c6c104"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libX11' 'libXext' 'libXfixes')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
	'--disable-malloc0returnsnull'
)
