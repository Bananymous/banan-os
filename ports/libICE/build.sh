#!/bin/bash ../install.sh

NAME='libICE'
VERSION='1.1.2'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libICE-$VERSION.tar.xz#974e4ed414225eb3c716985df9709f4da8d22a67a2890066bc6dfc89ad298625"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('xorgproto' 'xtrans')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
