#!/bin/bash ../install.sh

NAME='luajit'
VERSION='git'
DOWNLOAD_URL="https://github.com/LuaJIT/LuaJIT.git#dd3b24369ddfaa83c78cd6fdd91d157c8968ec8d"

configure() {
	make clean
}

build() {
	make -j$(nproc) CC=gcc BUILDMODE=dynamic CROSS=x86_64-pc-banan_os- || exit 1
}
