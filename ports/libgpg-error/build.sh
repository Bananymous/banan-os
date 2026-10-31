#!/bin/bash ../install.sh

NAME='libgpg-error'
VERSION='1.61'
DOWNLOAD_URL="https://www.gnupg.org/ftp/gcrypt/libgpg-error/libgpg-error-$VERSION.tar.bz2#7a85413f2bc354f4f8aa832b718af122e48965e9e0eb9012ee659c13c6385c93"
CONFIG_SUB=('build-aux/config.sub')
CONFIGURE_OPTIONS=(
	'--disable-static'
)
