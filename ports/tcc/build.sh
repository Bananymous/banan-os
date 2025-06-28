#!/bin/bash ../install.sh

NAME='tcc'
VERSION='0.9.27'
DOWNLOAD_URL="https://download.savannah.gnu.org/releases/tinycc/tcc-$VERSION.tar.bz2#de23af78fca90ce32dff2dd45b3432b2334740bb9bb7b05bf60fdbfc396ceb9c"

tcc_arch=$BANAN_ARCH
if [ $tcc_arch = 'i686' ]; then
	tcc_arch='i386'
fi

MAKE_BUILD_TARGETS=("cross-$tcc_arch $tcc_arch-libtcc1-usegcc=yes")
MAKE_INSTALL_TARGETS=("install-unx")

configure() {
	./configure \
		--prefix=/usr \
		--sysroot=$BANAN_SYSROOT \
		--cpu=$tcc_arch \
		--enable-cross \
		--cross-prefix=$BANAN_TOOLCHAIN_TRIPLE- \
		--sysincludepaths=/usr/include:/usr/lib/tcc/include \
		--libpaths=/usr/lib \
		--crtprefix=/usr/lib \
		--elfinterp=/usr/lib/DynamicLoader.so
}

post_install() {
	ln -sf $tcc_arch-tcc $BANAN_SYSROOT/usr/bin/tcc
}
