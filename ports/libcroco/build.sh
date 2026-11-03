#!/bin/bash ../install.sh

NAME='libcroco'
VERSION='0.6.13'
DOWNLOAD_URL="https://download.gnome.org/sources/libcroco/${VERSION%.*}/libcroco-$VERSION.tar.xz#767ec234ae7aa684695b3a735548224888132e063f92db585759b422570621d4"
DEPENDENCIES=('glib' 'libxml2')
CONFIG_SUB=('config.sub')
CONFIGURE=('configure')
CONFIGURE_OPTIONS=(
	'--disable-static'
	'--enable-shared'
)
