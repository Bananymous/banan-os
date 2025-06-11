#!/bin/bash ../install.sh

NAME='links'
VERSION='2.30'
DOWNLOAD_URL="http://links.twibright.com/download/links-$VERSION.tar.gz#7f0d54f4f7d1f094c25c9cbd657f98bc998311122563b1d757c9aeb1d3423b9e"
DEPENDENCIES=('ca-certificates' 'openssl' 'zlib' 'zstd' 'libpng' 'libjpeg' 'libtiff' 'libwebp')

post_configure() {
	config_defines=(
		'HAVE_PNG_CREATE_INFO_STRUCT'
		'HAVE_PNG_GET_BIT_DEPTH'
		'HAVE_PNG_GET_COLOR_TYPE'
		'HAVE_PNG_GET_GAMA'
		'HAVE_PNG_GET_IMAGE_HEIGHT'
		'HAVE_PNG_GET_IMAGE_WIDTH'
		'HAVE_PNG_GET_LIBPNG_VER'
		'HAVE_PNG_GET_SRGB'
		'HAVE_PNG_GET_VALID'
		'HAVE_PNG_SET_RGB_TO_GRAY'
		'HAVE_PNG_SET_STRIP_ALPHA'
		'HAVE_PNG_H'
		'HAVE_LIBPNG'

		'HAVE_JPEGLIB_H'
		'HAVE_LIBJPEG'
		'HAVE_JPEG'

		'HAVE_TIFFIO_H'
		'HAVE_LIBTIFF'
		'HAVE_TIFF'

		'HAVE_WEBPDECODERGBA'
		'HAVE_WEBPFREE'
		'HAVE_WEBP_DECODE_H'
		'HAVE_LIBWEBP'
		'HAVE_WEBP'

		'G'
	)

	for define in "${config_defines[@]}"; do
		sed -i "s|^/\* #undef $define \*/$|#define $define 1|" config.h
	done

	echo '#define GRDRV_BANAN_OS 1' >> config.h
}

build() {
	make -j$(nproc) -f Makefile.banan_os || exit 1
}

install() {
	cp -v links "$BANAN_SYSROOT/usr/bin/" || exit 1
}
