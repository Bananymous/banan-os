#!/bin/bash ../install.sh

NAME='libXcomposite'
VERSION='0.4.7'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXcomposite-$VERSION.tar.xz#8bdf310967f484503fa51714cf97bff0723d9b673e0eecbf92b3f97c060c8ccb"
CONFIG_SUB=('config.sub')
CONFIGURE=('configure')
DEPENDENCIES=('libX11' 'libXfixes')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
