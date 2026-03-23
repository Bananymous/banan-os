#!/bin/bash ../install.sh

NAME='libxkbfile'
VERSION='1.1.3'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libxkbfile-$VERSION.tar.xz#a9b63eea997abb9ee6a8b4fbb515831c841f471af845a09de443b28003874bec"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libX11' 'xkeyboard-config')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
