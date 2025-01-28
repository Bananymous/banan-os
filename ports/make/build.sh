#!/bin/bash ../install.sh

NAME='make'
VERSION='4.4.1'
DOWNLOAD_URL="https://ftp.gnu.org/gnu/make/make-$VERSION.tar.gz#dd16fb1d67bfab79a72f5e8390735c49e3e8e70b4945a15ab1f81ddb78658fb3"
CONFIGURE_OPTIONS=(
	'--with-sysroot=/'
	'--disable-nls'
	'--disable-posix-spawn'
	'--enable-year2038'
)
