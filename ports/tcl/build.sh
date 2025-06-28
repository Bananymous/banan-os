#!/bin/bash ../install.sh

NAME='tcl'
VERSION='8.6.12'
DOWNLOAD_URL="http://prdownloads.sourceforge.net/tcl/tcl$VERSION-src.tar.gz#26c995dd0f167e48b11961d891ee555f680c175f7173ff8cb829f4ebcde4c1a6"
TAR_CONTENT="tcl$VERSION"

pre_configure() {
	pushd unix || exit 1
}

post_configure() {
	popd
}

build() {
	make -C unix -j$(nproc) all || exit 1
}

install() {
	make -C unix install "DESTDIR=$BANAN_SYSROOT" || exit 1
}
