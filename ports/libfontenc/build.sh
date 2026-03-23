#!/bin/bash ../install.sh

NAME='libfontenc'
VERSION='1.1.8'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libfontenc-$VERSION.tar.xz#7b02c3d405236e0d86806b1de9d6868fe60c313628b38350b032914aa4fd14c6"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('zlib' 'xorgproto')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
