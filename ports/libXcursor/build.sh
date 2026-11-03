#!/bin/bash ../install.sh

NAME='libXcursor'
VERSION='1.2.3'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXcursor-$VERSION.tar.xz#fde9402dd4cfe79da71e2d96bb980afc5e6ff4f8a7d74c159e1966afb2b2c2c0"
CONFIG_SUB=('config.sub')
CONFIGURE=('configure')
DEPENDENCIES=('libX11' 'libXrender' 'libXfixes')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
