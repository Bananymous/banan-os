#!/bin/bash ../install.sh

NAME='nyancat'
VERSION='git'
DOWNLOAD_URL="https://github.com/klange/nyancat.git#1.5.2"

configure() {
	:
}

install() {
	cp src/nyancat "$BANAN_SYSROOT/usr/bin/"
}
