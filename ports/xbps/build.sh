#!/bin/bash ../install.sh

NAME='xbps'
VERSION='0.60.7'
DOWNLOAD_URL="https://github.com/void-linux/xbps/archive/refs/tags/$VERSION.tar.gz#ec8c2e4d595863b5748c10d9ada1d51ac1da6f910a9d31682acd1a477310c64e"
DEPENDENCIES=('libarchive' 'openssl')

pre_install() {
	# replace default void repo with our own
	echo 'repository=https://packages.bananymous.com/banan-os' > data/repod-main.conf

	# replace default void keys with our own
	rm data/*.plist
	cp "$BANAN_TOOLCHAIN_DIR/xbps-keys"/* data/
}
