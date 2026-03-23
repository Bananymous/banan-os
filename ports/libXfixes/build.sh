#!/bin/bash ../install.sh

NAME='libXfixes'
VERSION='6.0.2'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXfixes-$VERSION.tar.xz#39f115d72d9c5f8111e4684164d3d68cc1fd21f9b27ff2401b08fddfc0f409ba"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libX11')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
