#!/bin/bash ../install.sh

NAME='libXxf86vm'
VERSION='1.1.7'
DOWNLOAD_URL="https://xorg.freedesktop.org/archive/individual/lib/libXxf86vm-$VERSION.tar.xz#ae50c0f669e0af5a67cc4cd0f54f21d64a64d2660af883e80e95d3fe51b945d8"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libX11' 'libXext')
CONFIGURE=('configure')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
