#!/bin/bash ../install.sh

NAME='hyphen'
VERSION='2.8.9'
DOWNLOAD_URL="https://github.com/hunspell/hyphen/releases/download/v2.8.9/hyphen-$VERSION.tar.gz#783743daf477de8c4d16e3c74b4d2827377017718d8e17e2d9440210246f6abe"
CONFIG_SUB=('config.sub')
CONFIGURE_OPTIONS=(
	'--disable-static'
)
