#!/bin/bash ../install.sh

NAME='nghttp2'
VERSION='1.69.0'
DOWNLOAD_URL="https://github.com/nghttp2/nghttp2/releases/download/v$VERSION/nghttp2-$VERSION.tar.bz2#3f185f5b1c3e77885eb9cf3f2c4da4b1a9f73a24bf5957b829183ad137f82dc8"
CONFIG_SUB=('config.sub')
CONFIGURE_OPTIONS=(
	'--disable-static'
)
