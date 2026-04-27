#!/bin/bash ../install.sh

NAME='ca-certificates'
VERSION='2026.03.19'
DOWNLOAD_URL="https://curl.se/ca/cacert-${VERSION//./-}.pem#b6e66569cc3d438dd5abe514d0df50005d570bfc96c14dca8f768d020cb96171"

configure() {
	:
}

build() {
	:
}

install() {
	rm -rf "$BANAN_SYSROOT/etc/cacert/extracted"
	mkdir -p "$BANAN_SYSROOT/etc/cacert/extracted"

	cp -vf "../cacert-${VERSION//./-}.pem" "$BANAN_SYSROOT/etc/cacert/cacert.pem"
	awk '/-----BEGIN CERTIFICATE-----/ {c=1;n++} c {print > sprintf("cert%03d.pem", n)} /-----END CERTIFICATE-----/ {c=0}' "../cacert-${VERSION//./-}.pem"
	mv cert*.pem "$BANAN_SYSROOT/etc/cacert/extracted/"
}
