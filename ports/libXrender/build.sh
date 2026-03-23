#!/bin/bash ../install.sh

NAME='libXrender'
VERSION='0.9.12'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXrender-$VERSION.tar.xz#b832128da48b39c8d608224481743403ad1691bf4e554e4be9c174df171d1b97"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libX11')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
