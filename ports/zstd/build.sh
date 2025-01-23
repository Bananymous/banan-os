#!/bin/bash ../install.sh

NAME='zstd'
VERSION='1.5.6'
DOWNLOAD_URL="https://github.com/facebook/zstd/releases/download/v$VERSION/zstd-$VERSION.tar.gz#8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1"

configure() {
	:
}

build() {
	make -C lib -j$(nproc) lib-nomt || exit 1
}

install() {
	make -C lib install "DESTDIR=$BANAN_SYSROOT" PREFIX=/usr || exit 1
}
