#!/bin/bash ../install.sh

NAME='noto-fonts-cjk'
VERSION='git'
DOWNLOAD_URL="https://github.com/notofonts/noto-cjk.git#f8d157532fbfaeda587e826d4cd5b21a49186f7c"

configure() {
	:
}

build() {
	:
}

install() {
	mkdir -p "$DESTDIR/usr/share/fonts/noto-cjk" || exit 1
	cp -v */OTC/*.ttc "$DESTDIR/usr/share/fonts/noto-cjk/" || exit 1

	mkdir -p "$DESTDIR/usr/share/licenses" || exit 1
	cp -v Sans/LICENSE "$DESTDIR/usr/share/licenses/$NAME" || exit 1
}
