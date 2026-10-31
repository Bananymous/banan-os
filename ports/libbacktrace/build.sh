#!/bin/bash ../install.sh

NAME='libbacktrace'
VERSION='git'
DOWNLOAD_URL="https://github.com/ianlancetaylor/libbacktrace.git#5f4a6d4941de23180e7c412cc9ddf5ce440e3829"
DEPENDENCIES=('zlib' 'zstd')
CONFIG_SUB=('config.sub')
CONFIGURE_OPTIONS=(
	'--enable-shared'
	'--disable-static'
)
