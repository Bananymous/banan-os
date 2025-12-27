#!/bin/bash ../install.sh

NAME='ffmpeg'
VERSION='8.0.1'
DOWNLOAD_URL="https://ffmpeg.org/releases/ffmpeg-$VERSION.tar.xz#05ee0b03119b45c0bdb4df654b96802e909e0a752f72e4fe3794f487229e5a41"
DEPENDENCIES=('SDL2')
CONFIGURE_OPTIONS=(
	'--prefix=/usr'
	'--target-os=none'
	"--arch=$BANAN_ARCH"
	"--cc=$CC"
	"--cxx=$CXX"
	'--enable-cross-compile'
	'--enable-shared'
	'--enable-gpl'
)

configure() {
	./configure "${CONFIGURE_OPTIONS[@]}"
}
