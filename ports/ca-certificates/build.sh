#!/bin/bash ../install.sh

NAME='ca-certificates'
VERSION='2024-07-02'
DOWNLOAD_URL="https://curl.se/ca/cacert-$VERSION.pem#1bf458412568e134a4514f5e170a328d11091e071c7110955c9884ed87972ac9"

configure() {
	:
}

build() {
	:
}

install() {
	mkdir -p "$BANAN_SYSROOT/etc/ssl/certs"
	cp -v "../cacert-$VERSION.pem" "$BANAN_SYSROOT/etc/ssl/certs/ca-certificates.crt"
}
