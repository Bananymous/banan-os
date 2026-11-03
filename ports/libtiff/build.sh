#!/bin/bash ../install.sh

NAME='libtiff'
VERSION='4.7.2'
DOWNLOAD_URL="https://download.osgeo.org/libtiff/tiff-$VERSION.tar.xz#4996f0c4f93094719b1ca5c6279b20e588773ba8a247533e486416fb662ddb88"
TAR_CONTENT="tiff-$VERSION"
CONFIGURE=('configure')
DEPENDENCIES=('zlib' 'zstd' 'libjpeg-turbo')
CONFIGURE_OPTIONS=(
	'--disable-static'
)
