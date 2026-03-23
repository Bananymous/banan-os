#!/bin/bash ../install.sh

NAME='libXt'
VERSION='1.3.1'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXt-$VERSION.tar.xz#e0a774b33324f4d4c05b199ea45050f87206586d81655f8bef4dba434d931288"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libSM' 'libICE' 'libX11')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
