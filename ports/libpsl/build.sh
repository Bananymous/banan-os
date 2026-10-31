#!/bin/bash ../install.sh

NAME='libpsl'
VERSION='0.21.5'
DOWNLOAD_URL="https://github.com/rockdaboot/libpsl/releases/download/$VERSION/libpsl-$VERSION.tar.gz#1dcc9ceae8b128f3c0b3f654decd0e1e891afc6ff81098f227ef260449dae208"
CONFIG_SUB=('build-aux/config.sub')
CONFIGURE_OPTIONS=(
	'--disable-static'
)
