#!/bin/bash ../install.sh

NAME='expat'
VERSION='2.8.2'
DOWNLOAD_URL="https://github.com/libexpat/libexpat/releases/download/R_${VERSION//./_}/expat-$VERSION.tar.gz#ef7d1994f533c9e7343d6c19f31064fc8ebbcbcaa144be3812b4f43052a05f4c"
CONFIG_SUB=('conftools/config.sub')
CONFIGURE=('configure')
CONFIGURE_OPTIONS=(
	'--disable-static'
	'--with-dev-urandom'
)
