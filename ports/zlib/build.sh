#!/bin/bash ../install.sh

NAME='zlib'
VERSION='1.3.1'
DOWNLOAD_URL="https://www.zlib.net/zlib-$VERSION.tar.gz#9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23"

configure() {
	./configure --prefix=/usr --uname=banan_os
}
