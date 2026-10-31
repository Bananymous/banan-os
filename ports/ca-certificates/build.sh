#!/bin/bash ../install.sh

NAME='ca-certificates'
VERSION='2026.10.20'
DOWNLOAD_URL="https://curl.se/ca/cacert-${VERSION//./-}.pem#796f1389bfea13c0ed78f663b765baaff54a508e5483786bd6ece73c5e3e3cbe"

configure() {
	:
}

build() {
	:
}

install() {
	rm -rf "$DESTDIR/etc/cacert/extracted"
	mkdir -p "$DESTDIR/etc/cacert/extracted"

	cp -vf "../cacert-${VERSION//./-}.pem" "$DESTDIR/etc/cacert/cacert.pem"
	awk '/-----BEGIN CERTIFICATE-----/ {c=1;n++} c {print > sprintf("cert%03d.pem", n)} /-----END CERTIFICATE-----/ {c=0}' "../cacert-${VERSION//./-}.pem"
	mv cert*.pem "$DESTDIR/etc/cacert/extracted/"
}
