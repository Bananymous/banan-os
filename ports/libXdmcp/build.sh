#!/bin/bash ../install.sh

NAME='libXdmcp'
VERSION='1.1.5'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXdmcp-$VERSION.tar.xz#d8a5222828c3adab70adf69a5583f1d32eb5ece04304f7f8392b6a353aa2228c"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('xorgproto')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
