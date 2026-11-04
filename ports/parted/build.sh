#!/bin/bash ../install.sh

NAME='parted'
VERSION='3.8'
DOWNLOAD_URL="https://ftpmirror.gnu.org/gnu/parted/parted-$VERSION.tar.xz#a2b7811f47b0ddb1f7b1d0aa456f7c1270da70708ce231c2fe054c7199eafa63"
DEPENDENCIES=('libiconv' 'util-linux')
CONFIGURE_OPTIONS=(
	'--sbindir=/usr/bin'
	'--disable-device-mapper'
	'--without-readline'
	'ac_cv_c_bigendian=no'
	CFLAGS='-Wno-attributes'
)

pre_configure() {
	echo '#include_next <sys/types.h>' > lib/sys_types.in.h
	sed -i '/pragma weak pthread_self/d' lib/glthread/once.h
	sed -i 's/$LDFLAGS -rdynamic/$LDFLAGS/' configure
}

post_configure() {
	echo '#include <alloca.h>' >> lib/config.h
}
