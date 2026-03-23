#!/bin/bash ../install.sh

NAME='libxcb'
VERSION='1.17.0'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libxcb-$VERSION.tar.xz#599ebf9996710fea71622e6e184f3a8ad5b43d0e5fa8c4e407123c88a59a6d55"
CONFIG_SUB=('config.sub' 'build-aux/config.sub')
DEPENDENCIES=('xcb-proto' 'libXau' 'libXdmcp' 'libpthread-stubs')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
