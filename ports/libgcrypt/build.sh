#!/bin/bash ../install.sh

NAME='libgcrypt'
VERSION='1.12.2'
DOWNLOAD_URL="https://www.gnupg.org/ftp/gcrypt/libgcrypt/libgcrypt-$VERSION.tar.bz2#7ce33c2492221a0436f96a8500215e9f3e3dcb5fd26a757cd415e7a843babd5e"
DEPENDENCIES=('libgpg-error')
CONFIG_SUB=('build-aux/config.sub')
CONFIGURE_OPTIONS=(
	'--disable-static'
)
