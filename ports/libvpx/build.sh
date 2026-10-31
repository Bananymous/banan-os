#!/bin/bash ../install.sh

NAME='libvpx'
VERSION='1.16.0'
DOWNLOAD_URL="https://github.com/webmproject/libvpx/archive/refs/tags/v$VERSION.tar.gz#7a479a3c66b9f5d5542a4c6a1b7d3768a983b1e5c14c60a9396edc9b649e015c"
CONFIGURE_OPTIONS=(
	'--prefix=/usr'
	'--target=generic-gnu'
	'--disable-static'
	'--enable-shared'
)

configure() {
	./configure "${CONFIGURE_OPTIONS[@]}" || exit 1
}
