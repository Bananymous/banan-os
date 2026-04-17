#!/bin/bash ../install.sh

NAME='bzip2'
VERSION='1.0.8'
DOWNLOAD_URL="https://sourceware.org/pub/bzip2/bzip2-$VERSION.tar.gz#ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269"

configure() {
	:
}

build() {
	make -j$(nproc) -f Makefile-libbz2_so CC="$CC" || exit 1
}

install() {
	cp -v libbz2.so.$VERSION $BANAN_SYSROOT/usr/lib/ || exit 1
	ln -svf libbz2.so.$VERSION $BANAN_SYSROOT/usr/lib/libbz2.so || exit 1
	ln -svf libbz2.so.$VERSION $BANAN_SYSROOT/usr/lib/libbz2.so.1 || exit 1
	ln -svf libbz2.so.$VERSION $BANAN_SYSROOT/usr/lib/libbz2.so.1.0 || exit 1

	cp -v bzlib.h $BANAN_SYSROOT/usr/include/ || exit 1

	cat > $BANAN_SYSROOT/usr/lib/pkgconfig/bzip2.pc << EOF
prefix=/usr
exec_prefix=\${prefix}
bindir=\${exec_prefix}/bin
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: bzip2
Description: A file compression library
Version: $VERSION
Libs: -L\${libdir} -lbz2
Cflags: -I\${includedir}
EOF
}
