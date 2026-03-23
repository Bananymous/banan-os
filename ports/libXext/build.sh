#!/bin/bash ../install.sh

NAME='libXext'
VERSION='1.3.6'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXext-$VERSION.tar.xz#edb59fa23994e405fdc5b400afdf5820ae6160b94f35e3dc3da4457a16e89753"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libX11')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
	'--disable-malloc0returnsnull'
)
