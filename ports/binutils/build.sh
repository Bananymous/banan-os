#!/bin/bash ../install.sh

NAME='binutils'
VERSION='2.45'
DOWNLOAD_URL="https://ftpmirror.gnu.org/gnu/binutils/binutils-$VERSION.tar.gz#8a3eb4b10e7053312790f21ee1a38f7e2bbd6f4096abb590d3429e5119592d96"
DEPENDENCIES=('zlib' 'zstd')
MAKE_INSTALL_TARGETS=('install-strip')
CONFIGURE_OPTIONS=(
	"--target=$BANAN_TOOLCHAIN_TRIPLE"
	'--with-sysroot=/'
	"--with-build-sysroot=$BANAN_SYSROOT"
	'--enable-initfini-array'
	'--enable-shared'
	'--enable-lto'
	'--disable-nls'
	'--disable-werror'
)

pre_configure() {
	unset PKG_CONFIG_DIR
	unset PKG_CONFIG_SYSROOT_DIR
	unset PKG_CONFIG_LIBDIR
	unset PKG_CONFIG_PATH
}
