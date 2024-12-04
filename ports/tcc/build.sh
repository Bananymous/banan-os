#!/bin/bash ../install.sh

NAME='tcc'
VERSION='0.9.27'
DOWNLOAD_URL="https://download.savannah.gnu.org/releases/tinycc/tcc-$VERSION.tar.bz2#de23af78fca90ce32dff2dd45b3432b2334740bb9bb7b05bf60fdbfc396ceb9c"

configure() {
	./configure \
		--prefix=/usr \
		--sysroot=$BANAN_SYSROOT \
		--cpu=$BANAN_ARCH \
		--enable-cross \
		--cross-prefix=$BANAN_TOOLCHAIN_TRIPLE- \
		--sysincludepaths=/usr/include:/usr/lib/tcc/include \
		--libpaths=/usr/lib \
		--crtprefix=/usr/lib \
		--elfinterp=/usr/lib/DynamicLoader.so
}

build() {
	make -j$(nproc) cross-$BANAN_ARCH $BANAN_ARCH-libtcc1-usegcc=yes || exit 1
}

install() {
	make install-unx DESTDIR=$BANAN_SYSROOT || exit 1
	ln -sf $BANAN_ARCH-tcc $BANAN_SYSROOT/usr/bin/tcc
}
