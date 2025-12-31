#!/bin/bash ../install.sh

NAME='ca-certificates'
VERSION='2025-12-02'
DOWNLOAD_URL="https://curl.se/ca/cacert-$VERSION.pem#f1407d974c5ed87d544bd931a278232e13925177e239fca370619aba63c757b4"

configure() {
	:
}

build() {
	:
}

install() {
	mkdir -p "$BANAN_SYSROOT/etc/ssl/certs"
	cp -v "../cacert-$VERSION.pem" "$BANAN_SYSROOT/etc/ssl/certs/ca-certificates.crt"
	ln -svf "certs/ca-certificates.crt" "$BANAN_SYSROOT/etc/ssl/cert.pem"
}
