#!/bin/bash ../install.sh

NAME='halflife'
VERSION='git'
DOWNLOAD_URL="https://github.com/FWGS/hlsdk-portable.git#343b09bc4de15ecf310ab97e759bfdef6e883bd8"
DEPENDENCIES=('xash3d-fwgs')

configure() {
	./waf configure -T release || exit 1
}

build() {
	./waf build || exit 1
}

install() {
    ./waf install --destdir=$BANAN_SYSROOT/home/user/halflife || exit 1
}
