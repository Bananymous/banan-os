#!/bin/bash ../install.sh

NAME='libxshmfence'
VERSION='1.3.3'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libxshmfence-$VERSION.tar.xz#d4a4df096aba96fea02c029ee3a44e11a47eb7f7213c1a729be83e85ec3fde10"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('xorgproto')
CONFIGURE=('configure')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
