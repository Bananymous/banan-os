#!/bin/bash ../install.sh

NAME='libX11'
VERSION='1.8.12'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libX11-$VERSION.tar.xz#fa026f9bb0124f4d6c808f9aef4057aad65e7b35d8ff43951cef0abe06bb9a9a"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('xorgproto' 'xtrans' 'libxcb')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
