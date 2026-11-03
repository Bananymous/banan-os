#!/bin/bash ../install.sh

NAME='librsvg' # last pre-rust version
VERSION='2.40.21'
DOWNLOAD_URL="https://download.gnome.org/sources/librsvg/${VERSION%.*}/librsvg-$VERSION.tar.xz#f7628905f1cada84e87e2b14883ed57d8094dca3281d5bcb24ece4279e9a92ba"
DEPENDENCIES=('gdk-pixbuf' 'glib' 'libxml2' 'libcroco' 'cairo' 'pango')
CONFIG_SUB=('config.sub')
CONFIGURE=('configure')
CONFIGURE_OPTIONS=(
	'--disable-static'
	'--enable-shared'
	'--enable-pixbuf-loader'
	'--disable-introspection'
	'--disable-installed-tests'
	'--disable-always-build-tests'
)
