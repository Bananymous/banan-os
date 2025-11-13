#!/bin/bash ../install.sh

NAME='libvorbis'
VERSION='1.3.7'
DOWNLOAD_URL="https://github.com/xiph/vorbis/releases/download/v$VERSION/libvorbis-$VERSION.tar.gz#0e982409a9c3fc82ee06e08205b1355e5c6aa4c36bca58146ef399621b0ce5ab"
DEPENDENCIES=('libogg')
CONFIG_SUB=('config.sub')
