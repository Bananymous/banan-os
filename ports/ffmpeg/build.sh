#!/bin/bash ../install.sh

NAME='ffmpeg'
VERSION='9.0.2'
DOWNLOAD_URL="https://ffmpeg.org/releases/ffmpeg-$VERSION.tar.xz#8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e"
DEPENDENCIES=('sdl2-compat' 'openssl' 'dav1d' 'libass')
CONFIGURE_OPTIONS=(
	'--prefix=/usr'
	'--target-os=none'
	"--arch=$BANAN_ARCH"
	"--cc=$CC"
	"--cxx=$CXX"
	'--enable-cross-compile'
	'--enable-shared'
	'--disable-static'
	'--disable-doc'
	'--enable-gpl'
	'--enable-version3'
	'--enable-openssl'
	'--enable-libdav1d'
	'--enable-libass'
)

configure() {
	./configure "${CONFIGURE_OPTIONS[@]}" || exit 1
}
