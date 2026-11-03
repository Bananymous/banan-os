#!/bin/bash ../install.sh

NAME='libpng'
VERSION='1.6.58'
DOWNLOAD_URL="https://download.sourceforge.net/libpng/libpng-$VERSION.tar.gz#8c9b05b675ca7301a458df2c2e46f26e1d41ff36b8863f8c33530bc58c2e6225"
CONFIGURE=('configure')
DEPENDENCIES=('zlib')
CONFIGURE_OPTIONS=(
	'--disable-static'
)
