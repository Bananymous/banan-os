#!/bin/bash ../install.sh

NAME='sqlite3'
VERSION='3540100'
DOWNLOAD_URL="https://sqlite.org/2026/sqlite-autoconf-$VERSION.tar.gz#926ef8d3eab93fbf6e06011901273eec076b25594a4e1cdadd50c987f4f6d64e"
TAR_CONTENT="sqlite-autoconf-$VERSION"
CONFIG_SUB=('autosetup/autosetup-config.sub')
CONFIGURE_OPTIONS=(
	'--disable-static'
	'--soname=legacy'
)
