#!/bin/bash ../install.sh

NAME='libXaw'
VERSION='1.0.16'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXaw-$VERSION.tar.xz#731d572b54c708f81e197a6afa8016918e2e06dfd3025e066ca642a5b8c39c8f"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libX11' 'libXext' 'libXt' 'libXmu' 'libXpm')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
