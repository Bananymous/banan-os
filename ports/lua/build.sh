#!/bin/bash ../install.sh

NAME='lua'
VERSION='5.4.7'
DOWNLOAD_URL="https://www.lua.org/ftp/lua-$VERSION.tar.gz#9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30"

configure() {
	make clean
}

build() {
	make -j$(nproc) PLAT=posix CC="$CC" LIBS='$(SYSLIBS) $(MYLIBS)' || exit 1
}

install() {
	make install PLAT=posix INSTALL_TOP="$BANAN_SYSROOT/usr" || exit 1
}
