#!/bin/bash ../install.sh

NAME='libSM'
VERSION='1.2.6'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libSM-$VERSION.tar.xz#be7c0abdb15cbfd29ac62573c1c82e877f9d4047ad15321e7ea97d1e43d835be"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libICE')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
