#!/bin/bash ../install.sh

NAME='bochs'
VERSION='3.0'
DOWNLOAD_URL="https://sourceforge.net/projects/bochs/files/bochs/$VERSION/bochs-$VERSION.tar.gz#cb6f542b51f35a2cc9206b2a980db5602b7cd1b7cf2e4ed4f116acd5507781aa"
DEPENDENCIES=('SDL2')
CONFIG_SUB=('config.sub')
CONFIGURE_OPTIONS=(
	'--with-sdl2'
	'--enable-x86-64'
)
