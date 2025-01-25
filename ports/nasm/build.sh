#!/bin/bash ../install.sh

NAME='nasm'
VERSION='2.16.03'
DOWNLOAD_URL="https://www.nasm.us/pub/nasm/releasebuilds/$VERSION/nasm-$VERSION.tar.gz#5bc940dd8a4245686976a8f7e96ba9340a0915f2d5b88356874890e207bdb581"
CONFIGURE_OPTIONS=(
	'--disable-gdb'
)
