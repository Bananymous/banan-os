#!/bin/bash ../install.sh

NAME='libXau'
VERSION='1.0.12'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXau-$VERSION.tar.xz#74d0e4dfa3d39ad8939e99bda37f5967aba528211076828464d2777d477fc0fb"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('xorgproto')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
