#!/bin/bash ../install.sh

NAME='libXtst'
VERSION='1.2.5'
DOWNLOAD_URL="https://xorg.freedesktop.org/archive/individual/lib/libXtst-$VERSION.tar.xz#b50d4c25b97009a744706c1039c598f4d8e64910c9fde381994e1cae235d9242"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libX11' 'libXext' 'libXi')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
