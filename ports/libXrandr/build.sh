#!/bin/bash ../install.sh

NAME='libXrandr'
VERSION='1.5.5'
DOWNLOAD_URL="https://xorg.freedesktop.org/archive/individual/lib/libXrandr-$VERSION.tar.xz#72b922c2e765434e9e9f0960148070bd4504b288263e2868a4ccce1b7cf2767a"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libX11' 'libXext' 'libXrender')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
