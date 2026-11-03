#!/bin/bash ../install.sh

NAME='libXinerama'
VERSION='1.1.6'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXinerama-$VERSION.tar.xz#d00fc1599c303dc5cbc122b8068bdc7405d6fcb19060f4597fc51bd3a8be51d7"
CONFIG_SUB=('config.sub')
CONFIGURE=('configure')
DEPENDENCIES=('libX11' 'libXext')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
