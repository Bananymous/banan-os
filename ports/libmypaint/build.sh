#!/bin/bash ../install.sh

NAME='libmypaint'
VERSION='1.6.1'
DOWNLOAD_URL="https://github.com/mypaint/libmypaint/releases/download/v$VERSION/libmypaint-$VERSION.tar.xz#741754f293f6b7668f941506da07cd7725629a793108bb31633fb6c3eae5315f"
DEPENDENCIES=('glib' 'json-c')
CONFIG_SUB=('config.sub')
CONFIGURE=('configure')
CONFIGURE_OPTIONS=(
	'--disable-introspection'
	'--disable-i18n'
	'--disable-nls'
)
