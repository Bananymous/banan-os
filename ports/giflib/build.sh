#!/bin/bash ../install.sh

NAME='giflib'
VERSION='5.2.2'
DOWNLOAD_URL="https://sourceforge.net/projects/giflib/files/giflib-$VERSION.tar.gz#be7ffbd057cadebe2aa144542fd90c6838c6a083b5e8a9048b8ee3b66b29d5fb"

configure() {
	:
}

install() {
	make install-include PREFIX=/usr DESTDIR="$DESTDIR" || exit 1
	make install-bin     PREFIX=/usr DESTDIR="$DESTDIR" || exit 1
	make install-lib     PREFIX=/usr DESTDIR="$DESTDIR" || exit 1
	rm -f "$DESTDIR/usr/lib/libgif.a"
}
