#!/bin/bash ../install.sh

NAME='libXmu'
VERSION='1.2.1'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXmu-$VERSION.tar.xz#fcb27793248a39e5fcc5b9c4aec40cc0734b3ca76aac3d7d1c264e7f7e14e8b2"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libXt' 'libXext' 'libX11')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
