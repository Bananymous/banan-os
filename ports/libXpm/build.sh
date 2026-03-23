#!/bin/bash ../install.sh

NAME='libXpm'
VERSION='3.5.17'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXpm-$VERSION.tar.xz#64b31f81019e7d388c822b0b28af8d51c4622b83f1f0cb6fa3fc95e271226e43"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libX11')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
