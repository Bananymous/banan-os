#!/bin/bash ../install.sh

NAME='libXdamage'
VERSION='1.1.7'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXdamage-$VERSION.tar.xz#127067f521d3ee467b97bcb145aeba1078e2454d448e8748eb984d5b397bde24"
CONFIG_SUB=('config.sub')
CONFIGURE=('configure')
DEPENDENCIES=('libX11' 'libXfixes')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
