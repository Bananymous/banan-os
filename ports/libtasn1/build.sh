#!/bin/bash ../install.sh

NAME='libtasn1'
VERSION='4.21.0'
DOWNLOAD_URL="https://ftpmirror.gnu.org/gnu/libtasn1/libtasn1-$VERSION.tar.gz#1d8a444a223cc5464240777346e125de51d8e6abf0b8bac742ac84609167dc87"
CONFIGURE=('configure')
CONFIGURE_OPTIONS=(
	'--disable-static'
)

pre_configure() {
	echo '#include_next <sys/types.h>' > lib/gl/sys_types.in.h
	echo '#include_next <sys/types.h>' > src/gl/sys_types.in.h
}
